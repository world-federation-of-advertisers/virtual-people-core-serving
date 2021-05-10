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

#include "wfa/virtual_people/core/model/model_node_factory.h"

#include "absl/status/statusor.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/population_node_impl.h"

namespace wfa_virtual_people {

absl::StatusOr<std::unique_ptr<ModelNode>> ModelNodeFactory::NewModelNode(
    const CompiledNode& config) const {
  switch(config.type_case()) {
    case CompiledNode::TypeCase::kBranchNode:
      return absl::UnimplementedError("BranchNode is not implemented.");
    case CompiledNode::TypeCase::kStopNode:
      return absl::UnimplementedError("StopNode is not implemented.");
    case CompiledNode::TypeCase::kPopulationNode:
      return PopulationNodeImpl::Build(config);
    default:
      return absl::UnimplementedError("Node type is not set.");
  }
}

}  // namespace wfa_virtual_people
