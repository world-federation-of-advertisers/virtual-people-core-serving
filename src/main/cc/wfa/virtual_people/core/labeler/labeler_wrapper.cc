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

#include "wfa/virtual_people/core/labeler/labeler_wrapper.h"

#include <memory>

#include "absl/status/statusor.h"
#include "common_cpp/jni/jni_wrap.h"
#include "wfa/virtual_people/core/labeler/labeler.h"

using wfa::JniWrap;

namespace wfa_virtual_people {

namespace {
absl::StatusOr<LabelEventsResponse> LabelEvents(
    const LabelEventsRequest& request) {
  ASSIGN_OR_RETURN(std::unique_ptr<Labeler> labeler,
                   Labeler::Build(request.root_node()));
  LabelEventsResponse response;
  for (LabelerInput input : request.inputs()) {
    LabelerOutput output;
    RETURN_IF_ERROR(labeler->Label(input, output));
    output.clear_serialized_debug_trace();  // The debug trace does not
                                            // serialize properly.
    *response.add_outputs() = output;
  }
  return response;
}
}  // namespace

absl::StatusOr<std::string> LabelEventsWrapper(
    const std::string& serialized_request) {
  return JniWrap(serialized_request, LabelEvents);
}

}  // namespace wfa_virtual_people
