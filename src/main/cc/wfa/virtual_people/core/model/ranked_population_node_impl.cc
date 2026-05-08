// Copyright 2026 The Cross-Media Measurement Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "wfa/virtual_people/core/model/ranked_population_node_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common_cpp/macros/macros.h"
#include "src/farmhash.h"
#include "wfa/virtual_people/common/label.pb.h"
#include "wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/utils/distributed_consistent_hashing.h"
#include "wfa/virtual_people/core/model/utils/feistel.h"

namespace wfa_virtual_people {

namespace {

// Collapse the @quantum_label to a single label based on the probabilities, and
// merge to @output_label. Reused from PopulationNodeImpl.
absl::Status CollapseQuantumLabel(const QuantumLabel& quantum_label,
                                  absl::string_view seed_suffix,
                                  PersonLabelAttributes& output_label) {
  if (quantum_label.labels_size() == 0) {
    return absl::InvalidArgumentError("Empty quantum label.");
  }
  if (quantum_label.labels_size() != quantum_label.probabilities_size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "The sizes of labels and probabilities are different in quantum label ",
        quantum_label.DebugString()));
  }
  std::vector<DistributionChoice> distribution;
  distribution.reserve(quantum_label.probabilities_size());
  for (int i = 0; i < quantum_label.probabilities_size(); ++i) {
    distribution.emplace_back(
        DistributionChoice({i, quantum_label.probabilities(i)}));
  }
  ASSIGN_OR_RETURN(
      std::unique_ptr<DistributedConsistentHashing> hashing,
      DistributedConsistentHashing::Build(std::move(distribution)));
  int32_t index = hashing->Hash(absl::StrCat(
      "quantum-label-collapse-", quantum_label.seed(), seed_suffix));
  output_label.MergeFrom(quantum_label.labels(index));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<RankedPopulationNodeImpl>>
RankedPopulationNodeImpl::Build(const CompiledNode& node_config) {
  if (!node_config.has_ranked_population_node()) {
    return absl::InvalidArgumentError("This is not a ranked population node.");
  }
  const auto& config = node_config.ranked_population_node();

  if (config.pools_size() == 0) {
    return absl::InvalidArgumentError(
        "RankedPopulationNode must have at least one pool.");
  }

  // Compute total pool offset and size from the first pool.
  // For RankedPopulationNode, a single contiguous pool is expected.
  uint64_t pool_offset = config.pools(0).population_offset();
  uint64_t pool_size = 0;
  for (const auto& pool : config.pools()) {
    pool_size += pool.total_population();
  }

  if (pool_size == 0) {
    return absl::InvalidArgumentError(
        "RankedPopulationNode total pool size must be > 0.");
  }

  if (config.ranked_size() > pool_size) {
    return absl::InvalidArgumentError(absl::StrCat(
        "ranked_size (", config.ranked_size(),
        ") must not exceed pool_size (", pool_size, ")."));
  }

  return absl::make_unique<RankedPopulationNodeImpl>(
      node_config, config.random_seed(), config.ranked_size(),
      config.unranked_mode(), pool_offset, pool_size);
}

RankedPopulationNodeImpl::RankedPopulationNodeImpl(
    const CompiledNode& node_config, std::string random_seed,
    uint64_t ranked_size, UnrankedMode unranked_mode, uint64_t pool_offset,
    uint64_t pool_size)
    : ModelNode(node_config),
      random_seed_(std::move(random_seed)),
      ranked_size_(ranked_size),
      unranked_mode_(unranked_mode),
      pool_offset_(pool_offset),
      pool_size_(pool_size) {}

absl::Status RankedPopulationNodeImpl::Apply(LabelerEvent& event) const {
  if (event.virtual_person_activities_size() > 0) {
    return absl::InvalidArgumentError(
        "virtual_person_activities should only be created in leaf nodes.");
  }

  VirtualPersonActivity* activity = event.add_virtual_person_activities();
  uint64_t virtual_person_id;

  // Look up pre-computed rank from LabelerInput.rank_assignments.
  bool has_rank = false;
  uint64_t local_rank = 0;
  if (event.has_labeler_input()) {
    for (const auto& ra : event.labeler_input().rank_assignments()) {
      if (ra.pool_offset() == pool_offset_) {
        local_rank = ra.local_rank();
        has_rank = true;
        break;
      }
    }
  }

  if (has_rank && local_rank < ranked_size_) {
    // RANKED path: Feistel bijection — zero collisions.
    virtual_person_id =
        pool_offset_ + FeistelPermute(local_rank, ranked_size_, random_seed_);
  } else {
    // UNRANKED path: hash-based (mode-dependent scope).
    std::string seed_str =
        absl::StrCat(random_seed_, event.acting_fingerprint());
    uint64_t seed =
        util::Fingerprint64(seed_str.data(), seed_str.size());

    if (unranked_mode_ == DISJOINT) {
      uint64_t unranked_size = pool_size_ - ranked_size_;
      if (unranked_size == 0) {
        return absl::InvalidArgumentError(
            "DISJOINT mode with ranked_size == pool_size leaves no unranked "
            "space.");
      }
      virtual_person_id =
          pool_offset_ + ranked_size_ + (seed % unranked_size);
    } else {
      // FULL_POOL: hash into the entire pool.
      virtual_person_id = pool_offset_ + (seed % pool_size_);
    }
  }

  activity->set_virtual_person_id(virtual_person_id);

  // Collapse quantum labels from the event (same as PopulationNodeImpl).
  if (event.has_quantum_labels()) {
    std::string seed_suffix = std::to_string(virtual_person_id);
    for (const QuantumLabel& quantum_label :
         event.quantum_labels().quantum_labels()) {
      RETURN_IF_ERROR(CollapseQuantumLabel(quantum_label, seed_suffix,
                                           *activity->mutable_label()));
    }
  }
  // Merge classic label from the event.
  if (event.has_label()) {
    activity->mutable_label()->MergeFrom(event.label());
  }

  return absl::OkStatus();
}

}  // namespace wfa_virtual_people
