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
#include "common_cpp/testing/status_macros.h"
#include "common_cpp/testing/status_matchers.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"

#include <iostream>
#include <set>
#include <iostream>
#include <fstream>
#include <map>
#include <string>

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
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches {
            node {
              population_node {
                pools { population_offset: 10 total_population: 1 }
                random_seed: "TestPopulationNodeSeed1"
              }
            }
            chance: 0.4
          }
          branches {
            node {
              population_node {
                pools { population_offset: 20 total_population: 1 }
                random_seed: "TestPopulationNodeSeed1"
              }
            }
            chance: 0.6
          }
          random_seed: "TestBranchNodeSeed"
        }
      )pb",
      &config));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
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
  EXPECT_THAT(id_counts, UnorderedElementsAre(Pair(10, DoubleNear(0.4, 0.02)),
                                              Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(BranchNodeImplTest, TestApplyBranchWithNodeIndexByChance) {
  // The branch node has 2 branches.
  // One branch is selected with 40% chance, which is a population node always
  // assigns virtual person id 10.
  // The other branch is selected with 60% chance, which is a population node
  // always assigns virtual person id 20.
  CompiledNode branch_node_config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches { node_index: 2 chance: 0.4 }
          branches { node_index: 3 chance: 0.6 }
          random_seed: "TestBranchNodeSeed"
        }
      )pb",
      &branch_node_config));

  CompiledNode population_node_config_1;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestPopulationNode1"
        index: 2
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &population_node_config_1));

  CompiledNode population_node_config_2;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestPopulationNode2"
        index: 3
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed2"
        }
      )pb",
      &population_node_config_2));

  // Set up map from indexes to child nodes.
  absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>> node_refs;
  ASSERT_OK_AND_ASSIGN(node_refs[2],
                       ModelNode::Build(population_node_config_1));
  ASSERT_OK_AND_ASSIGN(node_refs[3],
                       ModelNode::Build(population_node_config_2));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> branch_node,
                       ModelNode::Build(branch_node_config, node_refs));

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
  EXPECT_THAT(id_counts, UnorderedElementsAre(Pair(10, DoubleNear(0.4, 0.02)),
                                              Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(BranchNodeImplTest, TestApplyBranchWithNodeByCondition) {
  // The branch node has 2 branches.
  // One branch is selected if person_country_code is "country_code_1", which is
  // a population node always assigns virtual person id 10.
  // The other branch is selected if person_country_code is "country_code_2",
  // which is a population node always assigns virtual person id 20.
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches {
            node {
              population_node {
                pools { population_offset: 10 total_population: 1 }
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
                pools { population_offset: 20 total_population: 1 }
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
      )pb",
      &config));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
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
  EXPECT_THAT(node->Apply(input_3),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestApplyBranchWithNodeIndexResolvedRecursively) {
  // The branch node has 2 branches.
  // One branch is selected with 40% chance, which is a branch node refers to a
  // single population node always assigning virtual person id 10.
  // The other branch is selected with 60% chance, which is a branch node refers
  // to a single population node always assigning virtual person id 20.
  CompiledNode branch_node_config_1;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode1"
        index: 1
        branch_node {
          branches { node_index: 2 chance: 0.4 }
          branches { node_index: 3 chance: 0.6 }
          random_seed: "TestBranchNodeSeed1"
        }
      )pb",
      &branch_node_config_1));

  CompiledNode branch_node_config_2;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode2"
        index: 2
        branch_node {
          branches { node_index: 4 chance: 1 }
          random_seed: "TestBranchNodeSeed2"
        }
      )pb",
      &branch_node_config_2));

  CompiledNode branch_node_config_3;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode3"
        index: 3
        branch_node {
          branches { node_index: 5 chance: 1 }
          random_seed: "TestBranchNodeSeed3"
        }
      )pb",
      &branch_node_config_3));

  CompiledNode population_node_config_1;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestPopulationNode1"
        index: 4
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &population_node_config_1));

  CompiledNode population_node_config_2;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestPopulationNode2"
        index: 5
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed2"
        }
      )pb",
      &population_node_config_2));

  // Set up map from indexes to child nodes.
  absl::flat_hash_map<uint32_t, std::unique_ptr<ModelNode>> node_refs;
  ASSERT_OK_AND_ASSIGN(node_refs[4],
                       ModelNode::Build(population_node_config_1));
  ASSERT_OK_AND_ASSIGN(node_refs[2],
                       ModelNode::Build(branch_node_config_2, node_refs));
  ASSERT_OK_AND_ASSIGN(node_refs[5],
                       ModelNode::Build(population_node_config_2));
  ASSERT_OK_AND_ASSIGN(node_refs[3],
                       ModelNode::Build(branch_node_config_3, node_refs));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> branch_node_1,
                       ModelNode::Build(branch_node_config_1, node_refs));

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
  EXPECT_THAT(id_counts, UnorderedElementsAre(Pair(10, DoubleNear(0.4, 0.02)),
                                              Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(BranchNodeImplTest, TestNoBranch) {
  CompiledNode config;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(R"pb(
                                                      name: "TestBranchNode"
                                                      index: 1
                                                      branch_node {}
                                                    )pb",
                                                    &config));
  EXPECT_THAT(ModelNode::Build(config).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestNoChildNode) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node { branches { chance: 1 } }
      )pb",
      &config));
  EXPECT_THAT(ModelNode::Build(config).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestNoSelectBy) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches {
            node {
              population_node {
                pools { population_offset: 10 total_population: 1 }
                random_seed: "TestPopulationNodeSeed1"
              }
            }
          }
        }
      )pb",
      &config));
  EXPECT_THAT(ModelNode::Build(config).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestDifferentSelectBy) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches {
            node {
              population_node {
                pools { population_offset: 10 total_population: 1 }
                random_seed: "TestPopulationNodeSeed1"
              }
            }
            chance: 0.5
          }
          branches {
            node {
              population_node {
                pools { population_offset: 20 total_population: 1 }
                random_seed: "TestPopulationNodeSeed1"
              }
            }
            condition {}
          }
        }
      )pb",
      &config));
  EXPECT_THAT(ModelNode::Build(config).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(BranchNodeImplTest, TestResolveChildReferencesIndexNotFound) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches { node_index: 2 chance: 1 }
          random_seed: "TestBranchNodeSeed"
        }
      )pb",
      &config));
  EXPECT_THAT(ModelNode::Build(config).status(),
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
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches {
            node {
              population_node {
                pools { population_offset: 10 total_population: 1 }
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
                pools { population_offset: 20 total_population: 1 }
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
                columns { person_country_code: "RAW_COUNTRY_1" }
                columns { person_country_code: "RAW_COUNTRY_2" }
                rows { person_country_code: "country_code_1" }
                rows { person_country_code: "country_code_2" }
                probabilities: 0.8
                probabilities: 0.2
                probabilities: 0.2
                probabilities: 0.8
              }
            }
          }
        }
      )pb",
      &config));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
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
    EXPECT_THAT(std::pair(person_country_code, id),
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
  EXPECT_THAT(id_counts_1,
              UnorderedElementsAre(Pair(10, DoubleNear(0.8, 0.02)),
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
    EXPECT_THAT(std::pair(person_country_code, id),
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
  EXPECT_THAT(id_counts_2,
              UnorderedElementsAre(Pair(10, DoubleNear(0.2, 0.02)),
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
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestBranchNode"
        index: 1
        branch_node {
          branches {
            node {
              population_node {
                pools { population_offset: 10 total_population: 1 }
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
                columns { person_country_code: "COUNTRY_1" }
                rows { person_country_code: "COUNTRY_2" }
                probabilities: 1
              }
            }
            updates {
              update_matrix {
                columns { person_country_code: "COUNTRY_2" }
                rows { person_country_code: "COUNTRY_3" }
                probabilities: 1
              }
            }
          }
        }
      )pb",
      &config));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

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

//break
TEST(BranchNodeImplTest, TestApplyUpdateMatricesInOrderTwo) {
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
name: "root"
branch_node {
  branches {
    condition {
     op: OR
     sub_filters {
      name: "labeler_input.profile_info.email_user_info.user_id"
      op: HAS
      }
     sub_filters {
      name: "labeler_input.profile_info.phone_user_info.user_id"
      op: HAS
      }
     sub_filters {
      name: "labeler_input.profile_info.proprietary_id_space_1_user_info.user_id"
      op: HAS
      }
    }
    node {
        name: "cookied"
        branch_node {
         branches {
            condition {
              op: TRUE
            }
            node {
              name: "cookied_pool"
               branch_node {
                  branches {
                    condition {
                     op: EQUAL
                     name:"labeler_input.geo.country_id"
                     value: "1"
                    }
                    node {
                      name: "cookied_pool-GB"
                      branch_node {
                        branches {
                          condition {
                           op: EQUAL
                           name:"labeler_input.user_agent"
                           value: "phone"
                          }
                          node {
                            name: "cookied_pool-GB-phone"
                            branch_node {
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-phone-Demo Bucket - F18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 11000000000
                                            total_population: 30478
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 11000030478
                                            total_population: 81437
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 11000111915
                                            total_population: 38085
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 11000150000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-phone-Demo Bucket - F25-34"
                                  branch_node {
                                    random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 12000000000
                                            total_population: 30478
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 12000030478
                                            total_population: 81437
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 12000111915
                                            total_population: 38085
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 12000150000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-phone-Demo Bucket - F35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 13000000000
                                            total_population: 40637
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 13000040637
                                            total_population: 108582
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 13000149219
                                            total_population: 50781
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 13000200000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-phone-Demo Bucket - M18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 14000000000
                                            total_population: 40637
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 14000040637
                                            total_population: 108582
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 14000149219
                                            total_population: 50781
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 14000200000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-phone-Demo Bucket - M25-34"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 15000000000
                                            total_population: 20319
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 15000020319
                                            total_population: 54291
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 15000074610
                                            total_population: 25390
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 15000100000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-phone-Demo Bucket - M35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 16000000000
                                            total_population: 20319
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 16000020319
                                            total_population: 54291
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 16000074610
                                            total_population: 25390
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 16000100000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                        branches {
                          condition {
                           op: EQUAL
                           name:"labeler_input.user_agent"
                           value: "email"
                          }
                          node {
                            name: "cookied_pool-GB-email"
                            branch_node {
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-email-Demo Bucket - F18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 11000000000
                                            total_population: 30478
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 11000030478
                                            total_population: 81437
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 11000111915
                                            total_population: 38085
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 11000150000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-email-Demo Bucket - F25-34"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 12000000000
                                            total_population: 30478
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 12000030478
                                            total_population: 81437
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 12000111915
                                            total_population: 38085
                                          }
                                        }
                                      }
                                    }
                                   branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 12000150000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-email-Demo Bucket - F35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 13000000000
                                            total_population: 40637
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 13000040637
                                            total_population: 108582
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 13000149219
                                            total_population: 50781
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 13000200000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-email-Demo Bucket - M18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 14000000000
                                            total_population: 40637
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 14000040637
                                            total_population: 108582
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 14000149219
                                            total_population: 50781
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 14000200000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-email-Demo Bucket - M25-34"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 15000000000
                                            total_population: 20319
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 15000020319
                                            total_population: 54291
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 15000074610
                                            total_population: 25390
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 15000100000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-GB-email-Demo Bucket - M35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 16000000000
                                            total_population: 20319
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 16000020319
                                            total_population: 54291
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 16000074610
                                            total_population: 25390
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 16000100000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                  branches {
                    condition {
                     op: EQUAL
                     name:"labeler_input.geo.country_id"
                     value: "2"
                    }
                    node {
                      name: "cookied_pool-US"
                      branch_node {
                        branches {
                          condition {
                           op: EQUAL
                           name:"labeler_input.user_agent"
                           value: "phone"
                          }
                          node {
                            name: "cookied_pool-US-phone"
                            branch_node {
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-phone-Demo Bucket - F18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 1000000000
                                            total_population: 2032
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 1000002032
                                            total_population: 5429
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 1000007461
                                            total_population: 2539
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 1000010000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-phone-Demo Bucket - F25-34"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 2000000000
                                            total_population: 4064
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 2000004064
                                            total_population: 10858
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 2000014922
                                            total_population: 5078
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 2000020000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-phone-Demo Bucket - F35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 3000000000
                                            total_population: 6096
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 3000006096
                                            total_population: 16287
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 3000022383
                                            total_population: 7617
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 3000030000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-phone-Demo Bucket - M18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 4000000000
                                            total_population: 8128
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 4000008128
                                            total_population: 21716
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 4000029844
                                            total_population: 10156
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 4000040000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-phone-Demo Bucket - M25-34"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 5000000000
                                            total_population: 10159
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 5000010159
                                            total_population: 27146
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 5000037305
                                            total_population: 12695
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 5000050000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-phone-Demo Bucket - M35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.1930248
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 6000000000
                                            total_population: 12191
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.5157664
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 6000012191
                                            total_population: 32575
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.24120784999999997
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 6000044766
                                            total_population: 15234
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000095
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 6000060000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                        branches {
                          condition {
                           op: EQUAL
                           name:"labeler_input.user_agent"
                           value: "email"
                          }
                          node {
                            name: "cookied_pool-US-email"
                            branch_node {
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-email-Demo Bucket - F18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 1000000000
                                            total_population: 2032
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 1000002032
                                            total_population: 5429
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 1000007461
                                            total_population: 2539
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 1000010000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-email-Demo Bucket - F25-34"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 2000000000
                                            total_population: 4064
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 2000004064
                                            total_population: 10858
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 2000014922
                                            total_population: 5078
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 2000020000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "2"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-email-Demo Bucket - F35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 3000000000
                                            total_population: 6096
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 3000006096
                                            total_population: 16287
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 3000022383
                                            total_population: 7617
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 3000030000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "18"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "24"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-email-Demo Bucket - M18-24"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 4000000000
                                            total_population: 8128
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 4000008128
                                            total_population: 21716
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 4000029844
                                            total_population: 10156
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 4000040000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "25"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "34"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-email-Demo Bucket - M25-34"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 5000000000
                                            total_population: 10159
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 5000010159
                                            total_population: 27146
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 5000037305
                                            total_population: 12695
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 5000050000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              branches {
                                condition {
                                  op: AND
                                   sub_filters {
                                    name: "corrected_demo.age.min_age"
                                    op: EQUAL
                                    value: "35"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.age.max_age"
                                    op: EQUAL
                                    value: "99"
                                  }
                                   sub_filters {
                                    name: "corrected_demo.gender"
                                    op: EQUAL
                                    value: "1"
                                  }
                                }
                                node {
                                  name: "cookied_pool-US-email-Demo Bucket - M35-99"
                                  branch_node {
                                     random_seed: "branch_node_random_seed"
                                    branches {
                                      chance: 0.015130494089495997
                                      node{
                                        population_node {
                                          pools {
                                            population_offset: 6000000000
                                            total_population: 12191
                                          }
                                        }

                                      }
                                    }
                                    branches {
                                      chance: 0.31475763773087995
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 6000012191
                                            total_population: 32575
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.6201114805866824
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 6000044766
                                            total_population: 15234
                                          }
                                        }
                                      }
                                    }
                                    branches {
                                      chance: 0.05000038759
                                      node {
                                        population_node {
                                          pools {
                                            population_offset: 6000060000
                                            total_population: 1
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
            }
          }
         updates {
            updates{
              conditional_merge {
                pass_through_non_matches: True
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "2"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "18"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "24"
                      }
                   }
                   update {
                      acting_demo_id_space: "phone"
                      acting_demo {
                         gender: 2
                         age {
                          min_age: 18
                          max_age: 24
                         }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "2"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "25"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "34"
                      }
                   }
                   update {
                      acting_demo_id_space: "phone"
                      acting_demo {
                        gender: 2
                        age {
                          min_age: 25
                          max_age: 34
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "2"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "35"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "99"
                      }
                   }
                   update {
                      acting_demo_id_space: "phone"
                      acting_demo {
                        gender: 2
                        age {
                          min_age: 35
                          max_age: 99
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "1"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "18"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "24"
                      }
                   }
                   update {
                      acting_demo_id_space: "phone"
                      acting_demo {
                        gender: 1
                        age {
                          min_age: 18
                          max_age: 24
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "1"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "25"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "34"
                      }
                   }
                   update {
                      acting_demo_id_space: "phone"
                      acting_demo {
                        gender: 1
                        age {
                          min_age: 25
                          max_age: 34
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "1"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "35"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.phone_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "99"
                      }
                   }
                   update {
                      acting_demo_id_space: "phone"
                      acting_demo {
                        gender: 1
                        age {
                          min_age: 35
                          max_age: 99
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "2"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "18"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "24"
                      }
                   }
                   update {
                      acting_demo_id_space: "email"
                      acting_demo {
                        gender: 2
                        age {
                          min_age: 18
                          max_age: 24
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "2"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "25"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "34"
                      }
                   }
                   update {
                      acting_demo_id_space: "email"
                      acting_demo {
                        gender: 2
                        age {
                          min_age: 25
                          max_age: 34
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "2"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "35"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "99"
                      }
                   }
                   update {
                      acting_demo_id_space: "email"
                      acting_demo {
                        gender: 2
                        age {
                          min_age: 35
                          max_age: 99
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "1"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "18"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "24"
                      }
                   }
                   update {
                      acting_demo_id_space: "email"
                      acting_demo {
                        gender: 1
                        age {
                          min_age: 18
                          max_age: 24
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "1"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "25"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "34"
                      }
                   }
                   update {
                      acting_demo_id_space: "email"
                      acting_demo {
                        gender: 1
                        age {
                          min_age: 25
                          max_age: 34
                        }
                      }
                   }
                }
                nodes {
                   condition {
                      op: AND
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.gender"
                         op: EQUAL
                         value: "1"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.min_age"
                         op: EQUAL
                         value: "35"
                      }
                      sub_filters {
                         name: "labeler_input.profile_info.email_user_info.demo.demo_bucket.age.max_age"
                         op: EQUAL
                         value: "99"
                      }
                   }
                   update {
                      acting_demo_id_space: "email"
                      acting_demo {
                        gender: 1
                        age {
                          min_age: 35
                          max_age: 99
                        }
                      }
                   }
                }
              }
            }
            updates{
              update_matrix {
               random_seed: "updater_matrix_random_seed"
              columns {
                 person_country_code: "GB"
                 acting_demo {
                    gender: 2
                    age {
                      min_age: 18
                      max_age: 24
                    }
                 }
              }
              columns {
                 person_country_code: "GB"
                 acting_demo {
                    gender: 2
                    age {
                      min_age: 25
                      max_age: 34
                    }
                 }
              }
              columns {
                 person_country_code: "GB"
                 acting_demo {
                    gender: 2
                    age {
                      min_age: 35
                      max_age: 99
                    }
                 }
              }
              columns {
                 person_country_code: "GB"
                 acting_demo {
                    gender: 1
                    age {
                      min_age: 18
                      max_age: 24
                    }
                 }
              }
              columns {
                 person_country_code: "GB"
                 acting_demo {
                    gender: 1
                    age {
                      min_age: 25
                      max_age: 34
                    }
                 }
              }
              columns {
                 person_country_code: "GB"
                 acting_demo {
                    gender: 1
                    age {
                      min_age: 35
                      max_age: 99
                    }
                 }
              }
              columns {
                 person_country_code: "US"
                 acting_demo {
                    gender: 2
                    age {
                      min_age: 18
                      max_age: 24
                    }
                 }
              }
              columns {
                 person_country_code: "US"
                 acting_demo {
                    gender: 2
                    age {
                      min_age: 25
                      max_age: 34
                    }
                 }
              }
              columns {
                 person_country_code: "US"
                 acting_demo {
                    gender: 2
                    age {
                      min_age: 35
                      max_age: 99
                    }
                 }
              }
              columns {
                 person_country_code: "US"
                 acting_demo {
                    gender: 1
                    age {
                      min_age: 18
                      max_age: 24
                    }
                 }
              }
              columns {
                 person_country_code: "US"
                 acting_demo {
                    gender: 1
                    age {
                      min_age: 25
                      max_age: 34
                    }
                 }
              }
              columns {
                 person_country_code: "US"
                 acting_demo {
                    gender: 1
                    age {
                      min_age: 35
                      max_age: 99
                    }
                 }
              }
              rows {
                 corrected_demo {
                    gender: 2
                    age {
                      min_age: 18
                      max_age: 24
                    }
                 }
              }
              rows {
                 corrected_demo {
                    gender: 2
                    age {
                      min_age: 25
                      max_age: 34
                    }
                 }
              }
              rows {
                 corrected_demo {
                    gender: 2
                    age {
                      min_age: 35
                      max_age: 99
                    }
                 }
              }
              rows {
                 corrected_demo {
                    gender: 1
                    age {
                      min_age: 18
                      max_age: 24
                    }
                 }
              }
              rows {
                 corrected_demo {
                    gender: 1
                    age {
                      min_age: 25
                      max_age: 34
                    }
                 }
              }
              rows {
                 corrected_demo {
                    gender: 1
                    age {
                      min_age: 35
                      max_age: 99
                    }
                 }
              }
              probabilities: 0.5
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.0
              probabilities: 0.05
              probabilities: 0.0
              probabilities: 0.5
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.0
              probabilities: 0.05
              probabilities: 0.0
              probabilities: 0.1
              probabilities: 0.55
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.0
              probabilities: 0.1
              probabilities: 0.55
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.0
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.6
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.6
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.65
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.65
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.7
              probabilities: 0.05
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.1
              probabilities: 0.7
              probabilities: 0.05
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.05
              probabilities: 0.05
              probabilities: 0.05
              probabilities: 0.75
              probabilities: 0.1
              probabilities: 0.05
              probabilities: 0.05
              probabilities: 0.05
              probabilities: 0.05
              probabilities: 0.75
              pass_through_non_matches: True
              }
            }
          }
        }
      }
   }
   branches {
      condition {
       op: NOT
       sub_filters {
        name: "labeler_input.profile_info.email_user_info.user_id"
        op: HAS
        }
       sub_filters {
        name: "labeler_input.profile_info.phone_user_info.user_id"
        op: HAS
        }
       sub_filters {
        name: "labeler_input.profile_info.proprietary_id_space_1_user_info.user_id"
        op: HAS
        }
      }
      node {
         name: "cookieless_pool"
         branch_node {
            branches {
               condition {
                 op: EQUAL
                 name:"labeler_input.geo.country_id"
                 value: "1"
               }
               node {
                  name: "cookieless_pool-GB"
                  branch_node {
                    branches {
                      chance: 0.014334152295311998
                      node{
                        population_node {
                          pools {
                            population_offset: 11000000000
                            total_population: 1524
                          }
                          pools {
                            population_offset: 12000000000
                            total_population: 1524
                          }
                          pools {
                            population_offset: 13000000000
                            total_population: 2032
                          }
                          pools {
                            population_offset: 14000000000
                            total_population: 2032
                          }
                          pools {
                            population_offset: 15000000000
                            total_population: 1016
                          }
                          pools {
                            population_offset: 16000000000
                            total_population: 1016
                          }
                        }
                      }
                    }
                    branches {
                      chance: 0.29819144627135996
                      node {
                        population_node {
                          pools {
                            population_offset: 11000001524
                            total_population: 4072
                          }
                          pools {
                            population_offset: 12000001524
                            total_population: 4072
                          }
                          pools {
                            population_offset: 13000002032
                            total_population: 5429
                          }
                          pools {
                            population_offset: 14000002032
                            total_population: 5429
                          }
                          pools {
                            population_offset: 15000001016
                            total_population: 2714
                          }
                          pools {
                            population_offset: 16000001016
                            total_population: 2714
                          }
                        }
                      }
                    }
                    branches {
                      chance: 0.587474034240015
                      node {
                        population_node {
                          pools {
                            population_offset: 11000005596
                            total_population: 1904
                          }
                          pools {
                            population_offset: 12000005596
                            total_population: 1904
                          }
                          pools {
                            population_offset: 13000007461
                            total_population: 2539
                          }
                          pools {
                            population_offset: 14000007461
                            total_population: 2539
                          }
                          pools {
                            population_offset: 15000003730
                            total_population: 1270
                          }
                          pools {
                            population_offset: 16000003730
                            total_population: 1270
                          }
                        }
                      }
                    }
                    branches {
                      chance: 0.10000036719
                      node {
                        population_node {
                          pools {
                            population_offset: 11000007500
                            total_population: 1
                          }
                          pools {
                            population_offset: 12000007500
                            total_population: 1
                          }
                          pools {
                            population_offset: 13000010000
                            total_population: 1
                          }
                          pools {
                            population_offset: 14000010000
                            total_population: 1
                          }
                          pools {
                            population_offset: 15000005000
                            total_population: 1
                          }
                          pools {
                            population_offset: 16000005000
                            total_population: 1
                          }
                        }
                      }
                    }
                  }
               }
            }
            branches {
               condition {
                 op: EQUAL
                 name:"labeler_input.geo.country_id"
                 value: "2"
               }
               node {
                  name: "cookieless_pool-US"
                  branch_node {
                    branches {
                      chance: 0.014334152295311998
                      node{
                        population_node {
                          pools {
                            population_offset: 1000000000
                            total_population: 102
                          }
                          pools {
                            population_offset: 2000000000
                            total_population: 203
                          }
                          pools {
                            population_offset: 3000000000
                            total_population: 305
                          }
                          pools {
                            population_offset: 4000000000
                            total_population: 406
                          }
                          pools {
                            population_offset: 5000000000
                            total_population: 508
                          }
                          pools {
                            population_offset: 6000000000
                            total_population: 610
                          }
                        }
                      }
                    }
                    branches {
                      chance: 0.29819144627135996
                      node {
                        population_node {
                          pools {
                            population_offset: 1000000102
                            total_population: 271
                          }
                          pools {
                            population_offset: 2000000203
                            total_population: 543
                          }
                          pools {
                            population_offset: 3000000305
                            total_population: 814
                          }
                          pools {
                            population_offset: 4000000406
                            total_population: 1086
                          }
                          pools {
                            population_offset: 5000000508
                            total_population: 1357
                          }
                          pools {
                            population_offset: 6000000610
                            total_population: 1628
                          }
                        }
                      }
                    }
                    branches {
                      chance: 0.587474034240015
                      node {
                        population_node {
                          pools {
                            population_offset: 1000000373
                            total_population: 127
                          }
                          pools {
                            population_offset: 2000000746
                            total_population: 254
                          }
                          pools {
                            population_offset: 3000001119
                            total_population: 381
                          }
                          pools {
                            population_offset: 4000001492
                            total_population: 508
                          }
                          pools {
                            population_offset: 5000001865
                            total_population: 635
                          }
                          pools {
                            population_offset: 6000002238
                            total_population: 762
                          }
                        }
                      }
                    }
                    branches {
                      chance: 0.10000036719
                      node {
                        population_node {
                          pools {
                            population_offset: 1000000500
                            total_population: 1
                          }
                          pools {
                            population_offset: 2000001000
                            total_population: 1
                          }
                          pools {
                            population_offset: 3000001500
                            total_population: 1
                          }
                          pools {
                            population_offset: 4000002000
                            total_population: 1
                          }
                          pools {
                            population_offset: 5000002500
                            total_population: 1
                          }
                          pools {
                            population_offset: 6000003000
                            total_population: 1
                          }
                        }
                      }
                    }
                  }
               }
            }
         }
      }
   }
}
    )pb", &config));

    std::cout << ModelNode::Build(config).status();

    ASSERT_OK_AND_ASSIGN(
            std::unique_ptr<ModelNode> node, ModelNode::Build(config));

    //ASSERT_OK_AND_ASSIGN(
    //        std::unique_ptr<ModelNode> node,ModelNodeFactory().NewModelNode(config));

    // Test for COUNTRY_1
    LabelerEvent example;

    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(
 labeler_input {
      user_agent: "phone"
      geo {
        country_id: 1
      }
      profile_info {
        phone_user_info {
          user_id: "123456"
          home_geo {
            country_id: 1
          }
          demo {
            demo_bucket {
              gender: 2
              age {
                min_age: 18
                max_age: 24
              }
            }
          }
        }
      }
    }
    person_country_code: "GB"
    acting_fingerprint: 1
    )pb", &example));

    std::map<int, int> demo_impressions_map;
    std::map<int, std::set<int>> demo_reach_map;

    for (int cookies = 0; cookies <= 100000; cookies += 10000){
        std::set<int> mySet;
        int counter = 0;
        for (int fingerprint = 0; fingerprint <= cookies; ++fingerprint) {
            LabelerEvent input(example);
            input.set_acting_fingerprint(fingerprint);
            auto res = node->Apply(input);
            //        std::cout << fingerprint << ":" << res << std::endl;
            if (res.ok()) {
                mySet.insert(input.virtual_person_activities(0).virtual_person_id());
                counter++;
                int key = input.corrected_demo().gender() + input.corrected_demo().age().min_age() + input.corrected_demo().age().max_age();
                demo_impressions_map[key]++;
                demo_reach_map[key].insert(input.virtual_person_activities(0).virtual_person_id());

                //Is there a faster way I can know which demo bucket it is?
                //            std::cout << input.virtual_person_activities(0).virtual_person_id() << std::endl;
            }
            else {
                std::cout << "failed" << std::endl;
            }
        }
        //    for (auto i = mySet.begin(); i != mySet.end(); i++)
        //        std::cout << *i << " ";
//        std::cout << mySet.size() << std::endl;
//        std::cout << counter;
        int demo_impression_total = 0;
        int demo_reach_total = 0;
        for (auto const &pair: demo_impressions_map) {
//            std::cout << "{" << pair.first << ": " << pair.second << "}\n";
            demo_impression_total += pair.second;
        }

        for (auto const &pair: demo_reach_map) {
            demo_reach_total += pair.second.size();
        }
        std::cout << cookies << "\n";

        double female_18_24 = (double) demo_impressions_map[44] / (double) demo_impression_total;
        double female_25_34 = (double) demo_impressions_map[61] / (double) demo_impression_total;
        double female_35_99 = (double) demo_impressions_map[136] / (double) demo_impression_total;
        double male_18_24 = (double) demo_impressions_map[43] / (double) demo_impression_total;
        double male_25_34 = (double) demo_impressions_map[60] / (double) demo_impression_total;
        double male_35_99 = (double) demo_impressions_map[135] / (double) demo_impression_total;
        std::cout << female_18_24 << " " << female_25_34 << " " << female_35_99  << " " << male_18_24 << " " << male_25_34 <<  " " << male_35_99 << "\n";

        female_18_24 = (double) demo_reach_map[44].size() / (double) demo_reach_total;
        female_25_34 = (double) demo_reach_map[61].size() / (double) demo_reach_total;
        female_35_99 = (double) demo_reach_map[136].size() / (double) demo_reach_total;
        male_18_24 = (double) demo_reach_map[43].size() / (double) demo_reach_total;
        male_25_34 = (double) demo_reach_map[60].size()/ (double) demo_reach_total;
        male_35_99 = (double) demo_reach_map[135].size() / (double) demo_reach_total;
        std::cout << female_18_24 << " " << female_25_34 << " " << female_35_99  << " " << male_18_24 << " " << male_25_34 <<  " " << male_35_99 << "\n";

    }
    std::cout << "DONE\n";
    //input.set_person_country_code("GB");
    //
    //auto current_acting_demo = input.get_mutable_acting_demo();
    //current_acting_demo.set_gender(2);
    //
    //auto age_range = current_acting_demo.get_mutable_age();
    //age_range.set_min_age(18);
    //age_range.set_max_age(24);
    //input.set_acting_fingerprint(1);

    //EXPECT_THAT(node->Apply(input), IsOk());
    //EXPECT_EQ(input.person_country_code(), "GB");
    //EXPECT_EQ(input.virtual_person_activities(0).virtual_person_id(), 10);

}

//break
}  // namespace
}  // namespace wfa_virtual_people
