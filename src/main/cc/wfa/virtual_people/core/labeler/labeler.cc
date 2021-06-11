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

#include "wfa/virtual_people/core/labeler/labeler.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/farmhash.h"
#include "src/main/proto/wfa/virtual_people/common/event.pb.h"
#include "src/main/proto/wfa/virtual_people/common/label.pb.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/measurement/common/macros.h"
#include "wfa/virtual_people/core/model/model_node.h"

namespace wfa_virtual_people {

absl::StatusOr<std::unique_ptr<Labeler>> Labeler::Build(
    const CompiledNode& root) {
  ASSIGN_OR_RETURN(
      std::unique_ptr<ModelNode> root_node,
      ModelNode::Build(root));
  return absl::make_unique<Labeler>(std::move(root_node));
}

absl::StatusOr<std::unique_ptr<Labeler>> Labeler::Build(
    const std::vector<CompiledNode>& nodes) {
  std::unique_ptr<ModelNode> root = nullptr;
  absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>> node_refs;

  for (const CompiledNode& node_config : nodes) {
    if (root) {
      return absl::InvalidArgumentError(
          "No node is allowed after the root node.");
    }
    if (node_config.has_index()) {
      if (node_refs.find(node_config.index()) != node_refs.end()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Duplicated indexes: ", node_config.index()));
      }
      ASSIGN_OR_RETURN(
          node_refs[node_config.index()],
          ModelNode::Build(node_config, node_refs));
    } else {
      ASSIGN_OR_RETURN(
          root, ModelNode::Build(node_config, node_refs));
    }
  }

  if (!root) {
    if (node_refs.empty()) {
      // This should never happen.
      return absl::InternalError("Cannot find root node.");
    }
    // We expect only 1 node in the node_refs map, which is the root node.
    root = std::move(node_refs.extract(node_refs.begin()).mapped());
  }

  if (!root) {
    // This should never happen.
    return absl::InternalError("Root is NULL.");
  }

  if (!node_refs.empty()) {
    return absl::InvalidArgumentError(
        "Some nodes are not in the model tree.");
  }

  return absl::make_unique<Labeler>(std::move(root));
}

void GenerateFingerprintForUserInfo(UserInfo& user_info) {
  if (user_info.has_user_id()) {
    user_info.set_user_id_fingerprint(util::Fingerprint64(user_info.user_id()));
  }
}

// Generates fingerprints for event_id and user_id.
// The default value of acting_fingerprint is the fingerprint of event_id.
void GenerateFingerprints(LabelerEvent& event) {
  LabelerInput* labeler_input = event.mutable_labeler_input();

  if (labeler_input->has_event_id()) {
    uint64_t event_id_fingerprint =
        util::Fingerprint64(labeler_input->event_id().id());
    labeler_input->mutable_event_id()->set_id_fingerprint(event_id_fingerprint);
    event.set_acting_fingerprint(event_id_fingerprint);
  }

  if (!labeler_input->has_profile_info()) {
    return;
  }
  ProfileInfo* profile_info = labeler_input->mutable_profile_info();
  if (profile_info->has_email_user_info()) {
    GenerateFingerprintForUserInfo(*profile_info->mutable_email_user_info());
  }
  if (profile_info->has_phone_user_info()) {
    GenerateFingerprintForUserInfo(*profile_info->mutable_phone_user_info());
  }
  if (profile_info->has_proprietary_id_space_1_user_info()) {
    GenerateFingerprintForUserInfo(
        *profile_info->mutable_proprietary_id_space_1_user_info());
  }
}

absl::Status Labeler::Label(
    const LabelerInput& input, LabelerOutput& output) const {
  // Prepare labeler event.
  LabelerEvent event;
  *event.mutable_labeler_input() = input;
  GenerateFingerprints(event);

  // Apply model.
  RETURN_IF_ERROR(root_->Apply(event));

  // Populate data to output.
  *output.mutable_people() = event.virtual_person_activities();
  return absl::OkStatus();
}

}  // namespace wfa_virtual_people
