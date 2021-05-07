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

#include "absl/container/flat_hash_map.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/model_node_factory.h"

namespace wfa_virtual_people {
namespace {

constexpr int kFingerprintNumber = 10000;

TEST(PopulationNodeImplTest, TestApply) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"PROTO(
      name: "TestPopulationNode"
      index: 1
      population_node {
        pools {
          population_offset: 10
          total_population: 3
        }
        pools {
          population_offset: 30
          total_population: 3
        }
        pools {
          population_offset: 20
          total_population: 4
        }
        random_seed: "TestRandomSeed"
      }
  )PROTO", &config));

  // The pools have 10 possible virtual person ids:
  //   10, 11, 12, 30, 31, 32, 20, 21, 22, 23
  absl::flat_hash_map<int64_t, int> id_counts = {
      {10, 0},
      {11, 0},
      {12, 0},
      {30, 0},
      {31, 0},
      {32, 0},
      {20, 0},
      {21, 0},
      {22, 0},
      {23, 0}
    };

  auto node_or = ModelNodeFactory().NewModelNode(config);
  EXPECT_TRUE(node_or.ok());
  std::unique_ptr<ModelNode> node = std::move(node_or.value());

  for (int fingerprint = 0; fingerprint < kFingerprintNumber; ++fingerprint) {
    LabelerEvent input;
    input.set_acting_fingerprint(fingerprint);
    EXPECT_TRUE(node->Apply(&input).ok());
    int64_t id = input.virtual_person_activities(0).virtual_person_id();
    // The virtual person id should be one of the 10 possible values.
    EXPECT_TRUE(id_counts.find(id) != id_counts.end());
    ++id_counts[id];
  }

  for (auto const& x : id_counts) {
    int count = x.second;
    // The expected ratio for getting a given virtual person id is 1 / 10 = 10%.
    // Absolute error more than 2% is very unlikely.
    EXPECT_NEAR(
        static_cast<double>(count) / static_cast<double>(kFingerprintNumber),
        0.1, 0.02);
  }
}

TEST(PopulationNodeImplTest, TestInvalidPools) {
  // The node is invalid as the total pools size is 0.
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"PROTO(
      name: "TestPopulationNode"
      index: 1
      population_node {
        pools {
          population_offset: 10
          total_population: 0
        }
        pools {
          population_offset: 30
          total_population: 0
        }
        pools {
          population_offset: 20
          total_population: 0
        }
        random_seed: "TestRandomSeed"
      }
  )PROTO", &config));

  auto node_or = ModelNodeFactory().NewModelNode(config);
  EXPECT_EQ(node_or.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace wfa_virtual_people
