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

#include "wfa/virtual_people/core/model/utils/distributed_consistent_hashing.h"

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace wfa_virtual_people {
namespace {

constexpr int kSeedNumber = 10000;

TEST(DistributedConsistentHashingTest, TestEmptyDistribution) {
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_EQ(hashing_or.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(DistributedConsistentHashingTest, TestZeroProbabilitiesSum) {
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution->emplace_back(std::make_pair(0, 0));
  distribution->emplace_back(std::make_pair(1, 0));
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_EQ(hashing_or.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(DistributedConsistentHashingTest, TestNegativeProbability) {
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution->emplace_back(std::make_pair(0, -1));
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_EQ(hashing_or.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(DistributedConsistentHashingTest, TestOutputDistribution) {
  // Distribution:
  // choice_id probability
  // 0         0.4
  // 1         0.2
  // 2         0.2
  // 3         0.2
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution->emplace_back(std::make_pair(0, 0.4));
  distribution->emplace_back(std::make_pair(1, 0.2));
  distribution->emplace_back(std::make_pair(2, 0.2));
  distribution->emplace_back(std::make_pair(3, 0.2));
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_TRUE(hashing_or.ok());
  std::unique_ptr<DistributedConsistentHashing> hashing =
      std::move(hashing_or.value());
  absl::flat_hash_map<int32_t, int> ouptut_counts = {
      {0, 0},
      {1, 0},
      {2, 0},
      {3, 0}};
  for (int seed = 0; seed < kSeedNumber; ++seed) {
    int32_t output = hashing->Hash(std::to_string(seed));
    // The output should always be one of [0, 1, 2, 3].
    EXPECT_TRUE(ouptut_counts.find(output) != ouptut_counts.end());
    ++ouptut_counts[output];
  }
  // Absolute error more than 2% is very unlikely.
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[0]) / static_cast<double>(kSeedNumber),
      0.4, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[1]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[2]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[3]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
}

TEST(DistributedConsistentHashingTest, TestNormalize) {
  // Distribution:
  // choice_id probability_before_normalized normalized_probability
  // 0         0.8                           0.4
  // 1         0.4                           0.2
  // 2         0.4                           0.2
  // 3         0.4                           0.2
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution->emplace_back(std::make_pair(0, 0.8));
  distribution->emplace_back(std::make_pair(1, 0.4));
  distribution->emplace_back(std::make_pair(2, 0.4));
  distribution->emplace_back(std::make_pair(3, 0.4));
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_TRUE(hashing_or.ok());
  std::unique_ptr<DistributedConsistentHashing> hashing =
      std::move(hashing_or.value());
  absl::flat_hash_map<int32_t, int> ouptut_counts = {
      {0, 0},
      {1, 0},
      {2, 0},
      {3, 0}};
  for (int seed = 0; seed < kSeedNumber; ++seed) {
    int32_t output = hashing->Hash(std::to_string(seed));
    // The output should always be one of [0, 1, 2, 3].
    EXPECT_TRUE(ouptut_counts.find(output) != ouptut_counts.end());
    ++ouptut_counts[output];
  }
  // Absolute error more than 2% is very unlikely.
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[0]) / static_cast<double>(kSeedNumber),
      0.4, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[1]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[2]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[3]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
}

TEST(DistributedConsistentHashingTest, TestZeroProbability) {
  // Distribution:
  // choice_id probability
  // 0         0
  // 1         1
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution->emplace_back(std::make_pair(0, 0));
  distribution->emplace_back(std::make_pair(1, 1));
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_TRUE(hashing_or.ok());
  std::unique_ptr<DistributedConsistentHashing> hashing =
      std::move(hashing_or.value());
  for (int seed = 0; seed < kSeedNumber; ++seed) {
    EXPECT_EQ(hashing->Hash(std::to_string(seed)), 1);
  }
}

TEST(DistributedConsistentHashingTest, TestNormalizeToZero) {
  // Distribution:
  // choice_id probability_before_normalized       normalized_probability
  // 0         std::numeric_limits<double>::min()  0
  // 1         std::numeric_limits<double>::max()  1
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution->emplace_back(
      std::make_pair(0, std::numeric_limits<double>::min()));
  distribution->emplace_back(
      std::make_pair(1, std::numeric_limits<double>::max()));
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_TRUE(hashing_or.ok());
  std::unique_ptr<DistributedConsistentHashing> hashing =
      std::move(hashing_or.value());
  for (int seed = 0; seed < kSeedNumber; ++seed) {
    EXPECT_EQ(hashing->Hash(std::to_string(seed)), 1);
  }
}

TEST(DistributedConsistentHashingTest, TestOutputDistribution_NonConsecutive) {
  // Distribution:
  // choice_id probability
  // 0         0.4
  // 2         0.2
  // 4         0.2
  // 6         0.2
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution->emplace_back(std::make_pair(0, 0.4));
  distribution->emplace_back(std::make_pair(2, 0.2));
  distribution->emplace_back(std::make_pair(4, 0.2));
  distribution->emplace_back(std::make_pair(6, 0.2));
  auto hashing_or =
      DistributedConsistentHashing::Build(std::move(distribution));
  EXPECT_TRUE(hashing_or.ok());
  std::unique_ptr<DistributedConsistentHashing> hashing =
      std::move(hashing_or.value());
  absl::flat_hash_map<int32_t, int> ouptut_counts = {
      {0, 0},
      {2, 0},
      {4, 0},
      {6, 0}};
  for (int seed = 0; seed < kSeedNumber; ++seed) {
    int32_t output = hashing->Hash(std::to_string(seed));
    // The output should always be one of [0, 2, 4, 6].
    EXPECT_TRUE(ouptut_counts.find(output) != ouptut_counts.end());
    ++ouptut_counts[output];
  }
  // Absolute error more than 2% is very unlikely.
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[0]) / static_cast<double>(kSeedNumber),
      0.4, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[2]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[4]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
  EXPECT_NEAR(
      static_cast<double>(ouptut_counts[6]) / static_cast<double>(kSeedNumber),
      0.2, 0.02);
}

TEST(DistributedConsistentHashingTest, TestOutputChangeCount) {
  // Distributions:
  // choice_id probability_1 probability_2
  // 0         0.4           0.2
  // 1         0.2           0.2
  // 2         0.2           0.2
  // 3         0.2           0.4
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution_1 =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution_1->emplace_back(std::make_pair(0, 0.4));
  distribution_1->emplace_back(std::make_pair(1, 0.2));
  distribution_1->emplace_back(std::make_pair(2, 0.2));
  distribution_1->emplace_back(std::make_pair(3, 0.2));
  auto hashing_or_1 =
      DistributedConsistentHashing::Build(std::move(distribution_1));
  EXPECT_TRUE(hashing_or_1.ok());
  std::unique_ptr<DistributedConsistentHashing> hashing_1 =
      std::move(hashing_or_1.value());
  std::unique_ptr<std::vector<std::pair<int32_t, double>>> distribution_2 =
      absl::make_unique<std::vector<std::pair<int32_t, double>>>();
  distribution_2->emplace_back(std::make_pair(0, 0.2));
  distribution_2->emplace_back(std::make_pair(1, 0.2));
  distribution_2->emplace_back(std::make_pair(2, 0.2));
  distribution_2->emplace_back(std::make_pair(3, 0.4));
  auto hashing_or_2 =
      DistributedConsistentHashing::Build(std::move(distribution_2));
  EXPECT_TRUE(hashing_or_2.ok());
  std::unique_ptr<DistributedConsistentHashing> hashing_2 =
      std::move(hashing_or_2.value());

  int diff_output_count = 0;
  for (int seed = 0; seed < kSeedNumber; ++seed) {
    int32_t output_1 = hashing_1->Hash(std::to_string(seed));
    int32_t output_2 = hashing_2->Hash(std::to_string(seed));
    if (output_1 != output_2) {
      ++diff_output_count;
    }
  }
  // The number of outputs different between 2 hashings is guaranteed to be less
  // than L1 distance of the 2 distributions, which is 40% of total counts here.
  EXPECT_LE(diff_output_count, kSeedNumber * 0.4);
  std::cerr << "diff_output_count: " << diff_output_count;
}

}  // namespace
}  // namespace wfa_virtual_people
