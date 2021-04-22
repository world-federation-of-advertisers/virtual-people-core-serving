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

#include "wfa/virtual_people/core/model/branch_node_impl.h"

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/measurement/common/macros.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/model_node_factory.h"
#include "wfa/virtual_people/core/model/utils/distributed_consistent_hashing.h"
#include "wfa/virtual_people/core/model/utils/field_filters_matcher.h"

namespace wfa_virtual_people {

// Converts @branch to a child node and appends to @child_nodes.
absl::Status AppendChildNode(
    const BranchNode::Branch& branch,
    const ModelNodeFactory& factory,
    std::vector<ChildNodeRef>* child_nodes) {
  if (branch.has_node_index()) {
    // Store the index of the child node. Will be resolved to the ModelNode
    // object in ResolveChildReferences.
    child_nodes->emplace_back(branch.node_index());
    return absl::OkStatus();
  } else if (branch.has_node()) {
    // Create the ModelNode object and store.
    child_nodes->emplace_back();
    ASSIGN_OR_RETURN(child_nodes->back(), factory.NewModelNode(branch.node()));
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      "BranchNode must have one of node_index and node.");
}

absl::StatusOr<std::unique_ptr<BranchNodeImpl>> BranchNodeImpl::Build(
    const CompiledNode& node_config) {
  if (!node_config.has_branch_node()) {
    return absl::InvalidArgumentError("This is not a branch node.");
  }
  const BranchNode& branch_node = node_config.branch_node();
  if (branch_node.branches_size() == 0) {
    return absl::InvalidArgumentError(
        "BranchNode must have at least 1 branch.");
  }

  // Converts each Branch to ChildNodeRef. Breaks if encounters any error
  // status.
  std::vector<ChildNodeRef> child_nodes;
  ModelNodeFactory factory;
  for (const BranchNode::Branch& branch : branch_node.branches()) {
    RETURN_IF_ERROR(AppendChildNode(branch, factory, &child_nodes));
  }

  // Checks if there is any
  //   @branch_node.branches.chance and
  //   @branch_node.branches.condition
  // Breaks if at least one @branch_node.branches has neither chance nor
  // condition.
  bool has_chance = false;
  bool has_condition = false;
  for (const BranchNode::Branch& branch : branch_node.branches()) {
    if (branch.has_chance()) {
      has_chance = true;
    } else if (branch.has_condition()) {
      has_condition = true;
    } else {
      return absl::InvalidArgumentError(
          "BranchNode must have one of chance and condition.");
    }
  }

  std::unique_ptr<DistributedConsistentHashing> hashing = nullptr;
  std::unique_ptr<FieldFiltersMatcher> matcher = nullptr;
  if (has_chance && !has_condition) {
    // All branches have chance set. The child node is selected by cosistent
    // hashing.
    std::vector<DistributionChoice> distribution;
    int index = 0;
    for (const BranchNode::Branch& branch : branch_node.branches()) {
      distribution.emplace_back(DistributionChoice({index, branch.chance()}));
      ++index;
    }
    ASSIGN_OR_RETURN(
        hashing, DistributedConsistentHashing::Build(std::move(distribution)));
  } else if (!has_chance && has_condition) {
    // All branches have condition set. The child node is selected by condition
    // matching.
    std::vector<const FieldFilterProto*> filter_configs;
    for (const BranchNode::Branch& branch : branch_node.branches()) {
      filter_configs.emplace_back(&branch.condition());
    }
    ASSIGN_OR_RETURN(matcher, FieldFiltersMatcher::Build(filter_configs));
  } else {
    return absl::InvalidArgumentError(
        "All branches must have same type of select_by set.");
  }

  return absl::make_unique<BranchNodeImpl>(
      node_config,
      std::move(child_nodes),
      std::move(hashing),
      branch_node.random_seed(),
      std::move(matcher));
}

BranchNodeImpl::BranchNodeImpl(
    const CompiledNode& node_config,
    std::vector<ChildNodeRef>&& child_nodes,
    std::unique_ptr<DistributedConsistentHashing> hashing,
    absl::string_view random_seed,
    std::unique_ptr<FieldFiltersMatcher> matcher):
    ModelNode(node_config),
    child_nodes_(std::move(child_nodes)),
    hashing_(std::move(hashing)),
    random_seed_(random_seed),
    matcher_(std::move(matcher)) {}

// If @child_node is set as an index, replaces it with the corresponding
// ModelNode object if found in @node_refs, and deletes the index / ModelNode
// pair from @node_refs.
// Also resolve the child references of the sub-tree of the @child_node.
absl::Status ResolveChildReference(
    ChildNodeRef& child_node,
    absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>>* node_refs) {
  if (child_node.index() == 0) {
    // The child node is referenced by node index. Need to resolve to the
    // ModelNode object.
    uint32_t node_index = std::get<0>(child_node);
    if (node_refs->find(node_index) == node_refs->end()) {
      return absl::InvalidArgumentError(
          "The ModelNode object of the child ndoe index is not provided.");
    }
    child_node.emplace<1>(std::move(node_refs->at(node_index)));
    // The owner of the corresponding ModelNode pointer is its parent node
    // now. Delete the key value pair.
    node_refs->erase(node_index);
  }

  if (child_node.index() != 1) {
    return absl::InternalError(
        "Neither node index nor ModelNode object is set for a child node.");
  }
  // Resolve the child node references of the sub tree here, because this node
  // is the only owner of the pointers to the child nodes.
  return std::get<1>(child_node)->ResolveChildReferences(node_refs);
}

absl::Status BranchNodeImpl::ResolveChildReferences(
    absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>>* node_refs) {
  for (ChildNodeRef& child_node : child_nodes_) {
    RETURN_IF_ERROR(ResolveChildReference(child_node, node_refs));
  }
  return absl::OkStatus();
}

absl::Status BranchNodeImpl::Apply(LabelerEvent* event) const {
  int selected_index = -1;
  if (hashing_) {
    // Select by chance.
    selected_index = hashing_->Hash(
        absl::StrCat(random_seed_, event->acting_fingerprint()));
  } else if (matcher_) {
    // Select by condition.
    selected_index = matcher_->GetFirstMatch(*event);
    if (selected_index == -1) {
      return absl::InvalidArgumentError(
          "No condition matches the input event.");
    }
  }
  if (child_nodes_[selected_index].index() != 1) {
    return absl::FailedPreconditionError(
        "The child node reference must be resolved before apply.");
  }
  return std::get<1>(child_nodes_[selected_index])->Apply(event);
}

}  // namespace wfa_virtual_people
