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

#include "wfa/virtual_people/core/model/model_node.h"

#include "src/main/proto/wfa/virtual_people/common/model.pb.h"

namespace wfa_virtual_people {

ModelNode::ModelNode(const CompiledNode& node_config):
    name_(node_config.name()),
    from_model_builder_config_(
        node_config.debug_info().directly_from_model_builder_config()) {}

}  // namespace wfa_virtual_people
