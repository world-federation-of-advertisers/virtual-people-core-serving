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

TEST(BranchNodeImplTest, TestApplyBranchWithNodeByChance) {
  // The branch node has 2 branches.
  // One branch is selected with 40% chance, which is a population node always
  // assigns virtual person id 10.
  // The other branch is selected with 60% chance, which is a population node
  // always assigns virtual person id 20.
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          node {
            population_node {
              pools {
                population_offset: 10
                total_population: 1
              }
              random_seed: "TestPopulationNodeSeed1"
            }
          }
          chance: 0.4
        }
        branches {
          node {
            population_node {
              pools {
                population_offset: 20
                total_population: 1
              }
              random_seed: "TestPopulationNodeSeed1"
            }
          }
          chance: 0.6
        }
        random_seed: "TestBranchNodeSeed"
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
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(id_counts, UnorderedElementsAre(
      Pair(10, DoubleNear(0.4, 0.02)),
      Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(BranchNodeImplTest, TestApplyBranchWithNodeIndexByChance) {
  // The branch node has 2 branches.
  // One branch is selected with 40% chance, which is a population node always
  // assigns virtual person id 10.
  // The other branch is selected with 60% chance, which is a population node
  // always assigns virtual person id 20.
  CompiledNode branch_node_config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          node_index: 2
          chance: 0.4
        }
        branches {
          node_index: 3
          chance: 0.6
        }
        random_seed: "TestBranchNodeSeed"
      }
  )pb", &branch_node_config));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> branch_node,
      ModelNodeFactory().NewModelNode(branch_node_config));

  // Set up map from indexes to child nodes.
  absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>> node_refs;

  CompiledNode population_node_config_1;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestPopulationNode1"
      index: 2
      population_node {
        pools {
          population_offset: 10
          total_population: 1
        }
        random_seed: "TestPopulationNodeSeed1"
      }
  )pb", &population_node_config_1));
  ASSERT_OK_AND_ASSIGN(
      node_refs[2],
      ModelNodeFactory().NewModelNode(population_node_config_1));

  CompiledNode population_node_config_2;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestPopulationNode2"
      index: 3
      population_node {
        pools {
          population_offset: 20
          total_population: 1
        }
        random_seed: "TestPopulationNodeSeed2"
      }
  )pb", &population_node_config_2));
  ASSERT_OK_AND_ASSIGN(
      node_refs[3],
      ModelNodeFactory().NewModelNode(population_node_config_2));

  EXPECT_THAT(branch_node->ResolveChildReferences(node_refs), IsOk());

  absl::flat_hash_map<int64_t, double> id_counts;
  for (int fingerprint = 0; fingerprint < kFingerprintNumber; ++fingerprint) {
    LabelerEvent input;
    input.set_acting_fingerprint(fingerprint);
    EXPECT_THAT(branch_node->Apply(input), IsOk());
    ++id_counts[input.virtual_person_activities(0).virtual_person_id()];
  }
  for (auto& [key, value] : id_counts) {
    value /= static_cast<double>(kFingerprintNumber);
  }
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(id_counts, UnorderedElementsAre(
      Pair(10, DoubleNear(0.4, 0.02)),
      Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(BranchNodeImplTest, TestApplyBranchWithNodeByCondition) {
  // The branch node has 2 branches.
  // One branch is selected if person_country_code is "country_code_1", which is
  // a population node always assigns virtual person id 10.
  // The other branch is selected if person_country_code is "country_code_2",
  // which is a population node always assigns virtual person id 20.
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          node {
            population_node {
              pools {
                population_offset: 10
                total_population: 1
              }
              random_seed: "TestPopulationNodeSeed1"
            }
          }
          condition {
            name: "person_country_code"
            op: EQUAL
            value: "country_code_1"
          }
        }
        branches {
          node {
            population_node {
              pools {
                population_offset: 20
                total_population: 1
              }
              random_seed: "TestPopulationNodeSeed1"
            }
          }
          condition {
            name: "person_country_code"
            op: EQUAL
            value: "country_code_2"
          }
        }
      }
  )pb", &config));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> node,
      ModelNodeFactory().NewModelNode(config));

  LabelerEvent input_1;
  input_1.set_person_country_code("country_code_1");
  EXPECT_THAT(node->Apply(input_1), IsOk());
  EXPECT_EQ(input_1.virtual_person_activities(0).virtual_person_id(), 10);

  LabelerEvent input_2;
  input_2.set_person_country_code("country_code_2");
  EXPECT_THAT(node->Apply(input_2), IsOk());
  EXPECT_EQ(input_2.virtual_person_activities(0).virtual_person_id(), 20);

  // No branch matches. Returns error status.
  LabelerEvent input_3;
  input_3.set_person_country_code("country_code_3");
  EXPECT_THAT(
      node->Apply(input_3),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestApplyBranchWithNodeIndexNotResolved) {
  // The branch node has 2 branches.
  // One branch is selected with 40% chance, which is a population node always
  // assigns virtual person id 10.
  // The other branch is selected with 60% chance, which is a population node
  // always assigns virtual person id 20.
  CompiledNode branch_node_config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          node_index: 2
          chance: 0.4
        }
        branches {
          node_index: 3
          chance: 0.6
        }
        random_seed: "TestBranchNodeSeed"
      }
  )pb", &branch_node_config));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> branch_node,
      ModelNodeFactory().NewModelNode(branch_node_config));

  LabelerEvent input;
  input.set_acting_fingerprint(0);
  EXPECT_THAT(
    branch_node->Apply(input),
    StatusIs(absl::StatusCode::kFailedPrecondition, ""));
}

TEST(BranchNodeImplTest, TestNoBranch) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {}
  )pb", &config));
  EXPECT_THAT(
      ModelNodeFactory().NewModelNode(config).status(),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestNoChildNode) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          chance: 0.5
        }
      }
  )pb", &config));
  EXPECT_THAT(
      ModelNodeFactory().NewModelNode(config).status(),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestNoSelectBy) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          node_index: 2
        }
      }
  )pb", &config));
  EXPECT_THAT(
      ModelNodeFactory().NewModelNode(config).status(),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestDifferentSelectBy) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          node_index: 2
          chance: 0.5
        }
        branches {
          node_index: 3
          condition {}
        }
      }
  )pb", &config));
  EXPECT_THAT(
      ModelNodeFactory().NewModelNode(config).status(),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestResolveChildReferencesIndexNotFound) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode"
      index: 1
      branch_node {
        branches {
          node_index: 2
          chance: 1
        }
        random_seed: "TestBranchNodeSeed"
      }
  )pb", &config));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> node,
      ModelNodeFactory().NewModelNode(config));

  absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>> node_refs;
  EXPECT_THAT(
      node->ResolveChildReferences(node_refs),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

}  // namespace
}  // namespace wfa_virtual_people