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
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "src/test/cc/testutil/matchers.h"
#include "src/test/cc/testutil/status_macros.h"
#include "wfa/virtual_people/core/model/model_node.h"

namespace wfa_virtual_people {
namespace {

using ::testing::AnyOf;
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
      ModelNode::Build(config));

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
      ModelNode::Build(branch_node_config));

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
      ModelNode::Build(population_node_config_1));

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
      ModelNode::Build(population_node_config_2));

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
      ModelNode::Build(config));

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

TEST(BranchNodeImplTest, TestApplyBranchWithNodeIndexResolvedRecursively) {
  // The branch node has 2 branches.
  // One branch is selected with 40% chance, which is a branch node refers to a
  // single population node always assigning virtual person id 10.
  // The other branch is selected with 60% chance, which is a branch node refers
  // to a single population node always assigning virtual person id 20.
  CompiledNode branch_node_config_1;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode1"
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
        random_seed: "TestBranchNodeSeed1"
      }
  )pb", &branch_node_config_1));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> branch_node_1,
      ModelNode::Build(branch_node_config_1));

  // Set up map from indexes to child nodes.
  absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>> node_refs;

  CompiledNode branch_node_config_2;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode2"
      index: 2
      branch_node {
        branches {
          node_index: 4
          chance: 1
        }
        random_seed: "TestBranchNodeSeed2"
      }
  )pb", &branch_node_config_2));
  ASSERT_OK_AND_ASSIGN(
      node_refs[2],
      ModelNode::Build(branch_node_config_2));

  CompiledNode branch_node_config_3;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestBranchNode3"
      index: 3
      branch_node {
        branches {
          node_index: 5
          chance: 1
        }
        random_seed: "TestBranchNodeSeed3"
      }
  )pb", &branch_node_config_3));
  ASSERT_OK_AND_ASSIGN(
      node_refs[3],
      ModelNode::Build(branch_node_config_3));

  CompiledNode population_node_config_1;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestPopulationNode1"
      index: 4
      population_node {
        pools {
          population_offset: 10
          total_population: 1
        }
        random_seed: "TestPopulationNodeSeed1"
      }
  )pb", &population_node_config_1));
  ASSERT_OK_AND_ASSIGN(
      node_refs[4],
      ModelNode::Build(population_node_config_1));

  CompiledNode population_node_config_2;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
      name: "TestPopulationNode2"
      index: 5
      population_node {
        pools {
          population_offset: 20
          total_population: 1
        }
        random_seed: "TestPopulationNodeSeed2"
      }
  )pb", &population_node_config_2));
  ASSERT_OK_AND_ASSIGN(
      node_refs[5],
      ModelNode::Build(population_node_config_2));

  EXPECT_THAT(branch_node_1->ResolveChildReferences(node_refs), IsOk());

  absl::flat_hash_map<int64_t, double> id_counts;
  for (int fingerprint = 0; fingerprint < kFingerprintNumber; ++fingerprint) {
    LabelerEvent input;
    input.set_acting_fingerprint(fingerprint);
    EXPECT_THAT(branch_node_1->Apply(input), IsOk());
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
      ModelNode::Build(branch_node_config));

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
      ModelNode::Build(config).status(),
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
      ModelNode::Build(config).status(),
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
      ModelNode::Build(config).status(),
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
      ModelNode::Build(config).status(),
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
      ModelNode::Build(config));

  absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>> node_refs;
  EXPECT_THAT(
      node->ResolveChildReferences(node_refs),
      StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestApplyUpdateMatrix) {
  // The branch node has 1 attributes updater and 2 branches.
  // The update matrix is
  //                     "RAW_COUNTRY_1" "RAW_COUNTRY_2"
  // "country_code_1"         0.8             0.2
  // "country_code_2"         0.2             0.8
  //
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
        updates {
          updates {
            update_matrix {
              columns {
                person_country_code: "RAW_COUNTRY_1"
              }
              columns {
                person_country_code: "RAW_COUNTRY_2"
              }
              rows {
                person_country_code: "country_code_1"
              }
              rows {
                person_country_code: "country_code_2"
              }
              probabilities: 0.8
              probabilities: 0.2
              probabilities: 0.2
              probabilities: 0.8
            }
          }
        }
      }
  )pb", &config));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> node,
      ModelNode::Build(config));

  // Test for RAW_COUNTRY_1
  absl::flat_hash_map<int64_t, double> id_counts_1;
  for (int fingerprint = 0; fingerprint < kFingerprintNumber; ++fingerprint) {
    LabelerEvent input;
    input.set_person_country_code("RAW_COUNTRY_1");
    input.set_acting_fingerprint(fingerprint);
    ASSERT_THAT(node->Apply(input), IsOk());
    // Fingerprint is not changed.
    EXPECT_EQ(input.acting_fingerprint(), fingerprint);
    absl::string_view person_country_code = input.person_country_code();
    int64_t id = input.virtual_person_activities(0).virtual_person_id();
    EXPECT_THAT(
        std::pair(person_country_code, id),
        AnyOf(Pair("country_code_1", 10), Pair("country_code_2", 20)));
    ++id_counts_1[id];
  }
  for (auto& [key, value] : id_counts_1) {
    value /= static_cast<double>(kFingerprintNumber);
  }
  // The selected column is
  //                     "RAW_COUNTRY_1"
  // "country_code_1"         0.8
  // "country_code_2"         0.2
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(id_counts_1, UnorderedElementsAre(
      Pair(10, DoubleNear(0.8, 0.02)),
      Pair(20, DoubleNear(0.2, 0.02))));

  // Test for RAW_COUNTRY_2
  absl::flat_hash_map<int64_t, double> id_counts_2;
  for (int fingerprint = 0; fingerprint < kFingerprintNumber; ++fingerprint) {
    LabelerEvent input;
    input.set_person_country_code("RAW_COUNTRY_2");
    input.set_acting_fingerprint(fingerprint);
    ASSERT_THAT(node->Apply(input), IsOk());
    absl::string_view person_country_code = input.person_country_code();
    int64_t id = input.virtual_person_activities(0).virtual_person_id();
    EXPECT_THAT(
        std::pair(person_country_code, id),
        AnyOf(Pair("country_code_1", 10), Pair("country_code_2", 20)));
    ++id_counts_2[id];
  }
  for (auto& [key, value] : id_counts_2) {
    value /= static_cast<double>(kFingerprintNumber);
  }
  // The selected column is
  //                     "RAW_COUNTRY_2"
  // "country_code_1"         0.2
  // "country_code_2"         0.8
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(id_counts_2, UnorderedElementsAre(
      Pair(10, DoubleNear(0.2, 0.02)),
      Pair(20, DoubleNear(0.8, 0.02))));
}

TEST(BranchNodeImplTest, TestApplyUpdateMatricesInOrder) {
  // The branch node has 2 attributes updater and 1 branches.
  // The 1st update matrix always changes person_country_code from COUNTRY_1 to
  // COUNTRY_2.
  // The 2nd update matrix always changes person_country_code from COUNTRY_2 to
  // COUNTRY_3.
  //
  // One branch is selected if person_country_code is "COUNTRY_3", which is a
  // population node always assigns virtual person id 10.
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
            value: "COUNTRY_3"
          }
        }
        updates {
          updates {
            update_matrix {
              columns {
                person_country_code: "COUNTRY_1"
              }
              rows {
                person_country_code: "COUNTRY_2"
              }
              probabilities: 1
            }
          }
          updates {
            update_matrix {
              columns {
                person_country_code: "COUNTRY_2"
              }
              rows {
                person_country_code: "COUNTRY_3"
              }
              probabilities: 1
            }
          }
        }
      }
  )pb", &config));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ModelNode> node, ModelNode::Build(config));

  // Test for COUNTRY_1
  for (int fingerprint = 0; fingerprint < kFingerprintNumber; ++fingerprint) {
    LabelerEvent input;
    input.set_person_country_code("COUNTRY_1");
    input.set_acting_fingerprint(fingerprint);
    ASSERT_THAT(node->Apply(input), IsOk());
    // Fingerprint is not changed.
    EXPECT_EQ(input.acting_fingerprint(), fingerprint);
    EXPECT_EQ(input.person_country_code(), "COUNTRY_3");
    EXPECT_EQ(input.virtual_person_activities(0).virtual_person_id(), 10);
  }
}

}  // namespace
}  // namespace wfa_virtual_people
