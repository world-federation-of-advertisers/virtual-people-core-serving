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

#include "wfa/virtual_people/core/model/utils/virtual_person_selector.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "src/test/cc/testutil/matchers.h"
#include "src/test/cc/testutil/status_macros.h"

namespace wfa_virtual_people {
namespace {

using ::testing::DoubleNear;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::wfa::StatusIs;

constexpr int kSeedNumber = 10000;

TEST(VirtualPersonSelectorTest, TestGetVirtualPersonId) {
  PopulationNode population_node;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        pools { population_offset: 10 total_population: 3 }
        pools { population_offset: 30 total_population: 3 }
        pools { population_offset: 20 total_population: 4 }
      )pb",
      &population_node));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<VirtualPersonSelector> selector,
                       VirtualPersonSelector::Build(population_node.pools()));

  absl::flat_hash_map<int64_t, double> id_counts;

  for (int seed = 0; seed < kSeedNumber; ++seed) {
    int64_t id = selector->GetVirtualPersonId(static_cast<uint64_t>(seed));
    ++id_counts[id];
  }
  for (auto& [key, value] : id_counts) {
    value /= static_cast<double>(kSeedNumber);
  }

  // The expected ratio for getting a given virtual person id is 1 / 10 = 10%.
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(
      id_counts,
      UnorderedElementsAre(
          Pair(10, DoubleNear(0.1, 0.02)), Pair(11, DoubleNear(0.1, 0.02)),
          Pair(12, DoubleNear(0.1, 0.02)), Pair(30, DoubleNear(0.1, 0.02)),
          Pair(31, DoubleNear(0.1, 0.02)), Pair(32, DoubleNear(0.1, 0.02)),
          Pair(20, DoubleNear(0.1, 0.02)), Pair(21, DoubleNear(0.1, 0.02)),
          Pair(22, DoubleNear(0.1, 0.02)), Pair(23, DoubleNear(0.1, 0.02))));
}

TEST(VirtualPersonSelectorTest, TestInvalidPools) {
  // This is invalid as the total pools size is 0.
  PopulationNode population_node;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        pools { population_offset: 10 total_population: 0 }
        pools { population_offset: 30 total_population: 0 }
        pools { population_offset: 20 total_population: 0 }
      )pb",
      &population_node));
  EXPECT_THAT(VirtualPersonSelector::Build(population_node.pools()).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

}  // namespace
}  // namespace wfa_virtual_people
