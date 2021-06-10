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

#include "wfa/virtual_people/core/model/attributes_updater/attributes_updater.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/attributes_updater/update_matrix_impl.h"

namespace wfa_virtual_people {

absl::StatusOr<std::unique_ptr<AttributesUpdaterInterface>>
AttributesUpdaterInterface::Build(
    const BranchNode::AttributesUpdater& config) {
  switch (config.update_case()) {
    case BranchNode::AttributesUpdater::UpdateCase::kUpdateMatrix:
      return UpdateMatrixImpl::Build(config.update_matrix());
    case BranchNode::AttributesUpdater::UpdateCase::kSparseUpdateMatrix:
      return absl::UnimplementedError("SparseUpdateMatrix is not implemented.");
    case BranchNode::AttributesUpdater::UpdateCase::kConditionalMerge:
      return absl::UnimplementedError("ConditionalMerge is not implemented.");
    case BranchNode::AttributesUpdater::UpdateCase::kUpdateTree:
      return absl::UnimplementedError("UpdateTree is not implemented.");
    default:
      return absl::InvalidArgumentError("config.update is not set.");
  }
}

}  // namespace wfa_virtual_people
