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

#ifndef WFA_VIRTUAL_PEOPLE_CORE_MODEL_ATTRIBUTES_UPDATER_ATTRIBUTES_UPDATER_H_
#define WFA_VIRTUAL_PEOPLE_CORE_MODEL_ATTRIBUTES_UPDATER_ATTRIBUTES_UPDATER_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"

namespace wfa_virtual_people {

// The C++ implementation of BranchNode.AttributesUpdater protobuf.
class AttributesUpdaterInterface {
 public:
  // Always use Build to get an AttributesUpdaterInterface object. Users should
  // not call the factory method or the constructor of the derived classes
  // directly.
  static absl::StatusOr<std::unique_ptr<AttributesUpdaterInterface>> Build(
      const BranchNode::AttributesUpdater& config);

  virtual ~AttributesUpdaterInterface() = default;

  AttributesUpdaterInterface(const AttributesUpdaterInterface&) = delete;
  AttributesUpdaterInterface& operator=(
      const AttributesUpdaterInterface&) = delete;

  // Applies the attributes updater to the @event.
  //
  // In general, there are 2 steps:
  // 1. Find the attributes to be merged into @event, by conditions matching and
  //    probabilities.
  // 2. Merge the attributes into @event.
  virtual absl::Status Update(LabelerEvent& event) const = 0;

 protected:
  AttributesUpdaterInterface() = default;
};

}  // namespace wfa_virtual_people

#endif  // WFA_VIRTUAL_PEOPLE_CORE_MODEL_ATTRIBUTES_UPDATER_ATTRIBUTES_UPDATER_H_
