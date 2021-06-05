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
#include "src/test/cc/testutil/matchers.h"
#include "src/test/cc/testutil/status_macros.h"
#include "wfa/virtual_people/core/model/model_node.h"
#include "wfa/virtual_people/core/model/model_node_factory.h"

namespace wfa_virtual_people {
namespace {

using ::testing::DoubleNear;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::wfa::IsOk;
using ::wfa::StatusIs;

constexpr int kFingerprintNumber = 10000;

TEST(PopulationNodeImplTest, TestApply) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
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
  )pb", &config));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> node,
      ModelNodeFactory().NewModelNode(config));

  absl::flat_hash_map<int64_t, double> id_counts;
  for (int fingerprint = 0; fingerprint < kFingerprintNumber; ++fingerprint) {
    LabelerEvent input;
    input.set_acting_fingerprint(fingerprint);
    EXPECT_THAT(node->Apply(input), IsOk());
    ++id_counts[input.virtual_person_activities(0).virtual_person_id()];
  }
  for (auto& [key, value] : id_counts) {
    value /= static_cast<double>(kFingerprintNumber);
  }

  // The expected ratio for getting a given virtual person id is 1 / 10 = 10%.
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(id_counts, UnorderedElementsAre(
      Pair(10, DoubleNear(0.1, 0.02)),
      Pair(11, DoubleNear(0.1, 0.02)),
      Pair(12, DoubleNear(0.1, 0.02)),
      Pair(30, DoubleNear(0.1, 0.02)),
      Pair(31, DoubleNear(0.1, 0.02)),
      Pair(32, DoubleNear(0.1, 0.02)),
      Pair(20, DoubleNear(0.1, 0.02)),
      Pair(21, DoubleNear(0.1, 0.02)),
      Pair(22, DoubleNear(0.1, 0.02)),
      Pair(23, DoubleNear(0.1, 0.02))));
}

TEST(PopulationNodeImplTest, TestInvalidPools) {
  // The node is invalid as the total pools size is 0.
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
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
  )pb", &config));

  EXPECT_THAT(
      ModelNodeFactory().NewModelNode(config).status(),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

}  // namespace
}  // namespace wfa_virtual_people
