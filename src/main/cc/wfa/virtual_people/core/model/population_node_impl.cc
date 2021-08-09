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
#include "src/main/proto/wfa/virtual_people/common/demographic.pb.h"
#include "src/main/proto/wfa/virtual_people/common/label.pb.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/utils/virtual_person_selector.h"

namespace wfa_virtual_people {

absl::StatusOr<std::unique_ptr<PopulationNodeImpl>> PopulationNodeImpl::Build(
    const CompiledNode& node_config) {
  if (!node_config.has_population_node()) {
    return absl::InvalidArgumentError("This is not a population node.");
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
  uint64_t seed = util::Fingerprint64(
      absl::StrCat(random_seed_, event.acting_fingerprint()));

  // Gets virtual person id from the pools.
  int64_t virtual_person_id =
      virtual_person_selector_->GetVirtualPersonId(seed);

  // Creates a new virtual_person_activities in @event and write the virtual
  // person id and label.
  // No virtual_person_activity should be added by previous nodes.
  if (event.virtual_person_activities_size() > 0) {
    return absl::InvalidArgumentError(
        "virtual_person_activities should only be created in leaf nodes.");
  }
  VirtualPersonActivity* virtual_person_activity =
      event.add_virtual_person_activities();
  virtual_person_activity->set_virtual_person_id(virtual_person_id);
  if (event.has_corrected_demo()) {
    *virtual_person_activity->mutable_label()->mutable_demo() =
        event.corrected_demo();
  }
  return absl::OkStatus();
}

}  // namespace wfa_virtual_people
