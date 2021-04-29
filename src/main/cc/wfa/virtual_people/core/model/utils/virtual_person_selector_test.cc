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

namespace wfa_virtual_people {
namespace {

constexpr int kSeedNumber = 10000;

TEST(VirtualPersonSelectorTest, TestGetVirtualPersonId) {
  PopulationNode population_node;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"PROTO(
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
  )PROTO", &population_node));
  absl::StatusOr<std::unique_ptr<VirtualPersonSelector>> selector_or =
      VirtualPersonSelector::Build(population_node.pools());
  EXPECT_TRUE(selector_or.ok());
  std::unique_ptr<VirtualPersonSelector> selector =
      std::move(selector_or.value());

  // The pools will have 10 possible virtual person ids:
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

  for (int seed  = 0; seed < kSeedNumber; ++seed) {
    int64_t id = selector->GetVirtualPersonId(static_cast<uint64_t>(seed));
    // The returned id should be one of the 10 possible values.
    EXPECT_TRUE(id_counts.find(id) != id_counts.end());
    ++id_counts[id];
  }

  for (auto const& x : id_counts) {
    int count = x.second;
    // The expectation is 1 / 10 = 10%.
    // Absolute error more than 2% is very unlikely.
    EXPECT_NEAR(static_cast<double>(count) / static_cast<double>(kSeedNumber),
                0.1, 0.02);
  }
}

TEST(VirtualPersonSelectorTest, TestInvalidPools) {
  PopulationNode population_node;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"PROTO(
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
  )PROTO", &population_node));
  absl::StatusOr<std::unique_ptr<VirtualPersonSelector>> selector_or =
      VirtualPersonSelector::Build(population_node.pools());
  EXPECT_EQ(selector_or.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace wfa_virtual_people
