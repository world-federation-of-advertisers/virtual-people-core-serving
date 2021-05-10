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

#ifndef WFA_VIRTUAL_PEOPLE_CORE_MODEL_MODEL_NODE_H_
#define WFA_VIRTUAL_PEOPLE_CORE_MODEL_MODEL_NODE_H_

#include <string>

#include "absl/status/status.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"

namespace wfa_virtual_people {

// This is the C++ implementation of CompiledNode proto. Each node in the model
// tree will be converted to a ModelNode.
// Except for debugging purposes, this should be used by VID Labeler only.
//
// This is a base class for all model node classes. Never add any behavior here.
// Only fields required for all model node classes should be added here.
class ModelNode {
 public:
  explicit ModelNode(const CompiledNode& node_config);
  virtual ~ModelNode() = default;

  // Applies the node to the @event.
  virtual absl::Status Apply(LabelerEvent* event) const = 0;

  ModelNode(const ModelNode&) = delete;
  ModelNode& operator=(const ModelNode&) = delete;

 private:
  std::string name_;
  bool from_model_builder_config_;
};

}  // namespace wfa_virtual_people

#endif  // WFA_VIRTUAL_PEOPLE_CORE_MODEL_MODEL_NODE_H_
