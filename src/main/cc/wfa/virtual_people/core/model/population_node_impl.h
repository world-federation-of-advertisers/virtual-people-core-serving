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

#ifndef WFA_VIRTUAL_PEOPLE_CORE_MODEL_POPULATION_NODE_IMPL_H_
#define WFA_VIRTUAL_PEOPLE_CORE_MODEL_POPULATION_NODE_IMPL_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/utils/virtual_person_selector.h"

namespace wfa_virtual_people {

// The implementation of the CompiledNode with population_node set.
//
// The field population_node in @node_config must be set.
// When applying the node, exactly one id will be selected from the pools in
// population_node, and assigned to virtual_person_activities in @event.
class PopulationNodeImpl : public ModelNode {
 public:
  // Always use Build to get a PopulationNodeImpl object. User should not call
  // the constructor below directly.
  static absl::StatusOr<std::unique_ptr<PopulationNodeImpl>> Build(
      const CompiledNode& node_config);

  // Never call the constructor directly.
  explicit PopulationNodeImpl(
      const CompiledNode& node_config,
      std::unique_ptr<VirtualPersonSelector> virtual_person_selector,
      const std::string& random_seed);
  ~PopulationNodeImpl() override {}

  absl::Status Apply(LabelerEvent* event) const override;

  PopulationNodeImpl(const PopulationNodeImpl&) = delete;
  PopulationNodeImpl& operator=(const PopulationNodeImpl&) = delete;

 private:
  std::unique_ptr<VirtualPersonSelector> virtual_person_selector_;
  const std::string random_seed_;
};

}  // namespace wfa_virtual_people

#endif  // WFA_VIRTUAL_PEOPLE_CORE_MODEL_POPULATION_NODE_IMPL_H_
