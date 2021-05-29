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

#include "wfa/virtual_people/core/model/attributes_updater/conditional_merge_impl.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/measurement/common/macros.h"
#include "wfa/virtual_people/common/field_filter/field_filter.h"
#include "wfa/virtual_people/core/model/utils/constants.h"
#include "wfa/virtual_people/core/model/utils/field_filters_matcher.h"

namespace wfa_virtual_people {

absl::StatusOr<std::unique_ptr<ConditionalMergeImpl>>
ConditionalMergeImpl::Build(const ConditionalMerge& config) {
  if (config.nodes_size() == 0) {
    return absl::InvalidArgumentError(absl::StrCat(
      "No nodes in ConditionalMerge: ", config.DebugString()));
  }

  // Converts each condition to a FieldFilter, and builds a FieldFiltersMatcher
  // with all the FieldFilters.
  std::vector<std::unique_ptr<FieldFilter>> filters;
  // Gets all the updates.
  std::vector<LabelerEvent> updates;
  for (const ConditionalMerge::ConditionalMergeNode& node : config.nodes()) {
    if (!node.has_condition()) {
      return absl::InvalidArgumentError(absl::StrCat(
        "No condition in the node in ConditionalMerge: ", node.DebugString()));
    }
    if (!node.has_update()) {
      return absl::InvalidArgumentError(absl::StrCat(
        "No update in the node in ConditionalMerge: ", node.DebugString()));
    }

    filters.emplace_back();
    ASSIGN_OR_RETURN(
        filters.back(),
        FieldFilter::New(LabelerEvent().GetDescriptor(), node.condition()));

    updates.emplace_back(node.update());
  }
  ASSIGN_OR_RETURN(
      std::unique_ptr<FieldFiltersMatcher> matcher,
      FieldFiltersMatcher::Build(std::move(filters)));

  return absl::make_unique<ConditionalMergeImpl>(
      std::move(matcher), std::move(updates),
      config.pass_through_non_matches());
}

absl::Status ConditionalMergeImpl::Update(LabelerEvent& event) const {
  int index = matcher_->GetFirstMatch(event);
  if (index == kNoMatchingIndex) {
    if (pass_through_non_matches_) {
      return absl::OkStatus();
    } else {
      return absl::InvalidArgumentError(absl::StrCat(
          "No node matching for event: ", event.DebugString()));
    }
  }
  event.MergeFrom(updates_[index]);
  return absl::OkStatus();
}

}  // namespace wfa_virtual_people
