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

#ifndef WFA_VIRTUAL_PEOPLE_CORE_MODEL_BRANCH_NODE_IMPL_H_
#define WFA_VIRTUAL_PEOPLE_CORE_MODEL_BRANCH_NODE_IMPL_H_

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/utils/distributed_consistent_hashing.h"
#include "wfa/virtual_people/core/model/utils/field_filters_matcher.h"

namespace wfa_virtual_people {

typedef std::variant<uint32_t, std::unique_ptr<ModelNode>> ChildNodeRef;

// The implementation of the CompiledNode with branch_node set.
//
// The field branch_node in @node_config must be set.
//
// Currently selecting child node by chance or condition is supported.
// TODO(@tcsnfkx): Implement attributes updater.
// TODO(@tcsnfkx): Implement multiplicity.
class BranchNodeImpl : public ModelNode {
 public:
  // Always use ModelNodeFactory.NewModelNode to get a ModelNode object.
  // Users should never call the factory function or constructor of the derived
  // class directly.
  //
  // Returns error status if any of the following happens:
  //   @node_config.branch_node is not set.
  //   @node_config.branch_node.branches is empty.
  //   Neither node_index nor node is set in at least one of the
  //     @node_config.branch_node.branches.
  //   Neither chance nor condition is set in at least one of the
  //     @node_config.branch_node.branches.
  //   At least one of @node_config.branch_node.branches has chance set, and at
  //     least one of @node_config.branch_node.branches has condition set.
  static absl::StatusOr<std::unique_ptr<BranchNodeImpl>> Build(
      const CompiledNode& node_config);

  explicit BranchNodeImpl(
      const CompiledNode& node_config,
      std::vector<ChildNodeRef>&& child_nodes,
      std::unique_ptr<DistributedConsistentHashing> hashing,
      absl::string_view random_seed,
      std::unique_ptr<FieldFiltersMatcher> matcher);
  ~BranchNodeImpl() override {}

  // Replaces all indexes in @child_nodes_ with the corresponding ModelNode
  // objects.
  // For any resolved index, the index / ModelNode object pair is deleted from
  // @node_refs.
  //
  // Calling this method will resolve the child references for any node in the
  // sub-tree starting from this node recursively.
  //
  // Returns error status if any of the following happens:
  //   Any index in @child_nodes_ does not have an entry in @node_refs.
  //   Neither index nor ModelNode is set in at least one entry in
  //     @child_nodes_.
  //   It returns error status when resolving the child references for any node
  //     in the sub-tree.
  //
  // TODO(@tcsnfkx): Resolve the child references of the UpdateTree if exists.
  absl::Status ResolveChildReferences(
      absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>>* node_refs
  ) override;

  // Uses @hashing_ or @matcher_ to select one of the ModelNodes in
  // @child_nodes_, and apply the selected ModelNode.
  absl::Status Apply(LabelerEvent* event) const override;

  BranchNodeImpl(const BranchNodeImpl&) = delete;
  BranchNodeImpl& operator=(const BranchNodeImpl&) = delete;

 private:
  // Include the child nodes in all the branches, in the same order as the
  // branches in @node_config.
  // When this node is built, each child node can be an index, or a ModelNode
  // object. Calling ResolveChildReferences will replace all indexes with
  // corresponding ModelNode objects.
  std::vector<ChildNodeRef> child_nodes_;

  // If chance is set in each branches in @node_config, hashing_ is set, and
  // used to select a child node with random_seed_ when Apply is called.
  std::unique_ptr<DistributedConsistentHashing> hashing_;
  const std::string random_seed_;

  // If condition is set in each branches in @node_config, matcher_ is set, and
  // used to select the first child node whose condition matches.
  std::unique_ptr<FieldFiltersMatcher> matcher_;
};

}  // namespace wfa_virtual_people

#endif  // WFA_VIRTUAL_PEOPLE_CORE_MODEL_BRANCH_NODE_IMPL_H_
