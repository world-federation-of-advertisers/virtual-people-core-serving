// Copyright 2022 The Cross-Media Measurement Authors
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

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "common_cpp/protobuf_util/textproto_io.h"
#include "common_cpp/testing/common_matchers.h"
#include "common_cpp/testing/status_macros.h"
#include "common_cpp/testing/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "wfa/virtual_people/common/event.pb.h"
#include "wfa/virtual_people/common/label.pb.h"
#include "wfa/virtual_people/common/model.pb.h"

namespace wfa_virtual_people {
namespace {

using ::testing::Eq;
using ::testing::Pointwise;
using ::wfa::EqualsProto;
using ::wfa::IsOk;
using ::wfa::ReadTextProtoFile;

const char kTestDataDir[] =
    "src/test/cc/wfa/virtual_people/core/labeler/test_data/";

TEST(LabelerWrapperTest, TestBuildFromRoot) {
  std::string model_path = "toy_model.textproto";
  std::vector<std::string> input_paths = {
      "labeler_input_01.textproto", "labeler_input_02.textproto",
      "labeler_input_03.textproto", "labeler_input_04.textproto",
      "labeler_input_05.textproto", "labeler_input_06.textproto",
      "labeler_input_07.textproto", "labeler_input_08.textproto",
      "labeler_input_09.textproto", "labeler_input_10.textproto",
      "labeler_input_11.textproto", "labeler_input_12.textproto"};
  std::vector<std::string> output_paths = {
      "labeler_output_01.textproto", "labeler_output_02.textproto",
      "labeler_output_03.textproto", "labeler_output_04.textproto",
      "labeler_output_05.textproto", "labeler_output_06.textproto",
      "labeler_output_07.textproto", "labeler_output_08.textproto",
      "labeler_output_09.textproto", "labeler_output_10.textproto",
      "labeler_output_11.textproto", "labeler_output_12.textproto"};
  CompiledNode root;
  EXPECT_THAT(ReadTextProtoFile(absl::StrCat(kTestDataDir, model_path), root),
              IsOk());
  LabelEventsRequest request;
  *request.mutable_root_node() = root;
  for (auto &input_path : input_paths) {
    LabelerInput input;
    EXPECT_THAT(
        ReadTextProtoFile(absl::StrCat(kTestDataDir, input_path), input),
        IsOk());
    *request.add_inputs() = input;
  }
  std::string serialized_request;
  request.SerializeToString(&serialized_request);
  ASSERT_OK_AND_ASSIGN(std::string serialized_response,
                       LabelEventsWrapper(serialized_request));
  LabelEventsResponse response;
  response.ParseFromString(serialized_response);
  ASSERT_EQ(response.outputs_size(), 12);

  std::vector<LabelerOutput> expected_outputs;
  std::transform(
      output_paths.begin(), output_paths.end(),
      std::back_inserter(expected_outputs), [](const std::string &output_path) {
        LabelerOutput expected_output;
        EXPECT_THAT(ReadTextProtoFile(absl::StrCat(kTestDataDir, output_path),
                                      expected_output),
                    IsOk());
        return expected_output;
      });
  EXPECT_THAT(response.outputs(), Pointwise(EqualsProto(), expected_outputs));
}

}  // namespace
}  // namespace wfa_virtual_people
