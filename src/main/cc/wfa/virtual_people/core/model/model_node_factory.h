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

#ifndef WFA_VIRTUAL_PEOPLE_CORE_MODEL_MODEL_NODE_FACTORY_H_
#define WFA_VIRTUAL_PEOPLE_CORE_MODEL_MODEL_NODE_FACTORY_H_

#include "absl/status/statusor.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"

namespace wfa_virtual_people {

class ModelNodeFactory {
 public:
  ModelNodeFactory() = default;

  // Creates a ModelNode based on the config.
  absl::StatusOr<std::unique_ptr<ModelNode>> NewModelNode(
      const CompiledNode& config) const;
};

}  // namespace wfa_virtual_people

#endif  // WFA_VIRTUAL_PEOPLE_CORE_MODEL_MODEL_NODE_FACTORY_H_
