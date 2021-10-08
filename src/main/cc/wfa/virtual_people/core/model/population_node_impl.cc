// Copyright 2021 The Cross-Media Measurement Authors
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

#include "wfa/virtual_people/core/model/population_node_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common_cpp/macros/macros.h"
#include "src/farmhash.h"
#include "wfa/virtual_people/common/demographic.pb.h"
#include "wfa/virtual_people/common/label.pb.h"
#include "wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/utils/distributed_consistent_hashing.h"
#include "wfa/virtual_people/core/model/utils/virtual_person_selector.h"

namespace wfa_virtual_people {

// Check if the @pools represent an empty population pool.
bool IsEmptyPopulationPool(
    const RepeatedPtrField<PopulationNode::VirtualPersonPool>& pools) {
  if (pools.size() != 1) {
    return false;
  }
  if (pools.Get(0).population_offset() == 0 &&
      pools.Get(0).total_population() == 0) {
    return true;
  }
  return false;
}

// Collapse the @quantum_label to a single label based on the probabilities, and
// output to @output_label.
absl::Status CollapseQuantumLabel(const QuantumLabel& quantum_label,
                                  PersonLabelAttributes& output_label,
                                  absl::string_view seed_suffix) {
  if (quantum_label.labels_size() == 0) {
    return absl::InvalidArgumentError("Empty quantum label.");
  }
  if (quantum_label.labels_size() != quantum_label.probabilities_size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "The sizes of labels and probabilities are different in quantum label ",
        quantum_label.DebugString()));
  }
  std::vector<DistributionChoice> distribution;
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

absl::StatusOr<std::unique_ptr<PopulationNodeImpl>> PopulationNodeImpl::Build(
    const CompiledNode& node_config) {
  if (!node_config.has_population_node()) {
    return absl::InvalidArgumentError("This is not a population node.");
  }
  if (IsEmptyPopulationPool(node_config.population_node().pools())) {
    return absl::make_unique<PopulationNodeImpl>(
        node_config, nullptr, node_config.population_node().random_seed());
  }
  ASSIGN_OR_RETURN(
      std::unique_ptr<VirtualPersonSelector> virtual_person_selector,
      VirtualPersonSelector::Build(node_config.population_node().pools()));
  return absl::make_unique<PopulationNodeImpl>(
      node_config, std::move(virtual_person_selector),
      node_config.population_node().random_seed());
}

PopulationNodeImpl::PopulationNodeImpl(
    const CompiledNode& node_config,
    std::unique_ptr<VirtualPersonSelector> virtual_person_selector,
    absl::string_view random_seed)
    : ModelNode(node_config),
      virtual_person_selector_(std::move(virtual_person_selector)),
      random_seed_(random_seed) {}

absl::Status PopulationNodeImpl::Apply(LabelerEvent& event) const {
  // Creates a new virtual_person_activities in @event and write the virtual
  // person id and label.
  // No virtual_person_activity should be added by previous nodes.
  if (event.virtual_person_activities_size() > 0) {
    return absl::InvalidArgumentError(
        "virtual_person_activities should only be created in leaf nodes.");
  }
  VirtualPersonActivity* virtual_person_activity =
      event.add_virtual_person_activities();

  // Only populate virtual_person_id when the pools is not an empty population
  // pool.
  if (virtual_person_selector_) {
    uint64_t seed = util::Fingerprint64(
        absl::StrCat(random_seed_, event.acting_fingerprint()));
    // Gets virtual person id from the pools.
    int64_t virtual_person_id =
        virtual_person_selector_->GetVirtualPersonId(seed);
    virtual_person_activity->set_virtual_person_id(virtual_person_id);
  }

  // Write to virtual_person_activity.label from classic label.
  if (event.has_label()) {
    virtual_person_activity->mutable_label()->MergeFrom(event.label());
  }
  // Write to virtual_person_activity.label from quantum labels.
  if (event.has_quantum_labels()) {
    std::string seed_suffix;
    if (virtual_person_activity->has_virtual_person_id()) {
      seed_suffix =
          std::to_string(virtual_person_activity->virtual_person_id());
    } else {
      seed_suffix = std::to_string(event.acting_fingerprint());
    }
    for (const QuantumLabel& quantum_label :
         event.quantum_labels().quantum_labels()) {
      RETURN_IF_ERROR(CollapseQuantumLabel(
          quantum_label, *virtual_person_activity->mutable_label(),
          seed_suffix));
    }
  }
  return absl::OkStatus();
}

}  // namespace wfa_virtual_people
