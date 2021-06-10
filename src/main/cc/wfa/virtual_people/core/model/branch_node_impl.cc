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
#include "wfa/virtual_people/core/model/attributes_updater/attributes_updater.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/utils/constants.h"
#include "wfa/virtual_people/core/model/utils/distributed_consistent_hashing.h"
#include "wfa/virtual_people/core/model/utils/field_filters_matcher.h"

namespace wfa_virtual_people {

// Converts @branch to a child node and appends to @child_nodes.
absl::Status AppendChildNode(
    const BranchNode::Branch& branch,
    std::vector<std::unique_ptr<ModelNode>>& child_nodes,
    absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>>& node_refs) {
  if (branch.has_node_index()) {
    // The child node is referenced by node index. Need to resolve to the
    // ModelNode object.
    // The owner of the corresponding ModelNode pointer will be its parent node.
    // Gets the key value pair, and deletes them from the map.
    auto node = node_refs.extract(branch.node_index());
    if (node.empty()) {
      return absl::InvalidArgumentError(
          "The ModelNode object of the child node index is not provided.");
    }
    child_nodes.emplace_back(std::move(node.mapped()));
    return absl::OkStatus();
  }
  if (branch.has_node()) {
    // Create the ModelNode object and store.
    child_nodes.emplace_back();
    ASSIGN_OR_RETURN(
        child_nodes.back(), ModelNode::Build(branch.node(), node_refs));
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      "BranchNode must have one of node_index and node.");
}

// Builds hashing based on the chances distribution.
// All branches must have chance set.
absl::StatusOr<std::unique_ptr<DistributedConsistentHashing>> BuildHashing(
    const google::protobuf::RepeatedPtrField<BranchNode::Branch>& branches) {
  std::vector<DistributionChoice> distribution;
  int index = 0;
  for (const BranchNode::Branch& branch : branches) {
    distribution.emplace_back(DistributionChoice({index, branch.chance()}));
    ++index;
  }
  return DistributedConsistentHashing::Build(std::move(distribution));
}

// Builds matcher based on the conditions.
// All branches must have condition set.
absl::StatusOr<std::unique_ptr<FieldFiltersMatcher>> BuildMatcher(
    const google::protobuf::RepeatedPtrField<BranchNode::Branch>& branches) {
  std::vector<const FieldFilterProto*> filter_configs;
  for (const BranchNode::Branch& branch : branches) {
    filter_configs.emplace_back(&branch.condition());
  }
  return FieldFiltersMatcher::Build(filter_configs);
}

absl::StatusOr<std::unique_ptr<BranchNodeImpl>> BranchNodeImpl::Build(
    const CompiledNode& node_config,
    absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>>& node_refs) {
  if (!node_config.has_branch_node()) {
    return absl::InvalidArgumentError("This is not a branch node.");
  }
  const BranchNode& branch_node = node_config.branch_node();
  if (branch_node.branches_size() == 0) {
    return absl::InvalidArgumentError(
        "BranchNode must have at least 1 branch.");
  }

  // Converts each Branch to ModelNode. Breaks if encounters any error status.
  std::vector<std::unique_ptr<ModelNode>> child_nodes;
  for (const BranchNode::Branch& branch : branch_node.branches()) {
    RETURN_IF_ERROR(AppendChildNode(branch, child_nodes, node_refs));
  }

  // If all @branch_node.branches have chance, use chance.
  // If all @branch_node.branches have condition, use condition.
  // Else return error.
  BranchNode::Branch::SelectByCase select_by_case =
      branch_node.branches(0).select_by_case();
  for (const BranchNode::Branch& branch : branch_node.branches()) {
    if (select_by_case != branch.select_by_case()) {
      return absl::InvalidArgumentError(
          "All branches should use the same select_by type.");
    }
  }

  std::unique_ptr<DistributedConsistentHashing> hashing = nullptr;
  std::unique_ptr<FieldFiltersMatcher> matcher = nullptr;
  switch (select_by_case) {
    case BranchNode::Branch::kChance: {
      ASSIGN_OR_RETURN(hashing, BuildHashing(branch_node.branches()));
      break;
    }
    case BranchNode::Branch::kCondition: {
      ASSIGN_OR_RETURN(matcher, BuildMatcher(branch_node.branches()));
      break;
    }
    default:
      // No select_by is set.
      return absl::InvalidArgumentError(
          "BranchNode must have one of chance and condition.");
  }

  // Builds attributes updaters.
  std::vector<std::unique_ptr<AttributesUpdaterInterface>> updaters;
  if (branch_node.has_updates()) {
    for (const BranchNode::AttributesUpdater& config :
            branch_node.updates().updates()) {
      updaters.emplace_back();
      ASSIGN_OR_RETURN(
          updaters.back(), AttributesUpdaterInterface::Build(config));
      if (!updaters.back()) {
        return absl::InternalError(absl::StrCat(
            "Failed to build AttributesUpdater with config: ",
            config.DebugString()));
      }
    }
  }

  return absl::make_unique<BranchNodeImpl>(
      node_config,
      std::move(child_nodes),
      std::move(hashing),
      branch_node.random_seed(),
      std::move(matcher),
      std::move(updaters));
}

BranchNodeImpl::BranchNodeImpl(
    const CompiledNode& node_config,
    std::vector<std::unique_ptr<ModelNode>>&& child_nodes,
    std::unique_ptr<DistributedConsistentHashing> hashing,
    absl::string_view random_seed,
    std::unique_ptr<FieldFiltersMatcher> matcher,
    std::vector<std::unique_ptr<AttributesUpdaterInterface>>&& updaters):
    ModelNode(node_config),
    child_nodes_(std::move(child_nodes)),
    hashing_(std::move(hashing)),
    random_seed_(random_seed),
    matcher_(std::move(matcher)),
    updaters_(std::move(updaters)) {}

absl::Status BranchNodeImpl::Apply(LabelerEvent& event) const {
  // Applies attributes updaters.
  for (auto& updater : updaters_) {
    RETURN_IF_ERROR(updater->Update(event));
  }

  int selected_index = kNoMatchingIndex;
  if (hashing_) {
    // Select by chance.
    selected_index = hashing_->Hash(
        absl::StrCat(random_seed_, event.acting_fingerprint()));
  } else if (matcher_) {
    // Select by condition.
    selected_index = matcher_->GetFirstMatch(event);
    if (selected_index == kNoMatchingIndex) {
      return absl::InvalidArgumentError(
          "No condition matches the input event.");
    }
  } else {
    return absl::InternalError("No select options is set for the BranchNode.");
  }

  return child_nodes_[selected_index]->Apply(event);
}

}  // namespace wfa_virtual_people
