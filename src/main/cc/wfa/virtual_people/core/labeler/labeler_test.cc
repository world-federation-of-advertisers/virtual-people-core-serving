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

#include "wfa/virtual_people/core/labeler/labeler.h"

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "common_cpp/testing/status_macros.h"
#include "common_cpp/testing/status_matchers.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "src/main/proto/wfa/virtual_people/common/event.pb.h"
#include "src/main/proto/wfa/virtual_people/common/label.pb.h"
#include "src/main/proto/wfa/virtual_people/common/model.pb.h"

namespace wfa_virtual_people {
namespace {

using ::testing::DoubleNear;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::wfa::IsOk;
using ::wfa::StatusIs;

constexpr int kEventIdNumber = 10000;

TEST(LabelerTest, TestBuildFromRoot) {
  // A model with
  // * 40% probability to assign virtual person id 10
  // * 60% probability to assign virtual person id 20
  CompiledNode root;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestNode1"
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
                random_seed: "TestPopulationNodeSeed2"
              }
            }
            chance: 0.6
          }
          random_seed: "TestBranchNodeSeed"
        }
      )pb",
      &root));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Labeler> labeler, Labeler::Build(root));

  absl::flat_hash_map<int64_t, double> vpid_counts;
  for (int event_id = 0; event_id < kEventIdNumber; ++event_id) {
    LabelerInput input;
    input.mutable_event_id()->set_id(std::to_string(event_id));
    LabelerOutput output;
    EXPECT_THAT(labeler->Label(input, output), IsOk());
    ++vpid_counts[output.people(0).virtual_person_id()];
  }
  for (auto& [key, value] : vpid_counts) {
    value /= static_cast<double>(kEventIdNumber);
  }
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(vpid_counts,
              UnorderedElementsAre(Pair(10, DoubleNear(0.4, 0.02)),
                                   Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(LabelerTest, TestBuildFromNodesRootWithIndex) {
  // All nodes, including root node, have index set.
  // A model with
  // * 40% probability to assign virtual person id 10
  // * 60% probability to assign virtual person id 20
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 2
        name: "TestNode2"
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 3
        name: "TestNode3"
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 1
        name: "TestNode1"
        branch_node {
          branches { node_index: 2 chance: 0.4 }
          branches { node_index: 3 chance: 0.6 }
          random_seed: "TestBranchNodeSeed"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Labeler> labeler, Labeler::Build(nodes));

  absl::flat_hash_map<int64_t, double> vpid_counts;
  for (int event_id = 0; event_id < kEventIdNumber; ++event_id) {
    LabelerInput input;
    input.mutable_event_id()->set_id(std::to_string(event_id));
    LabelerOutput output;
    EXPECT_THAT(labeler->Label(input, output), IsOk());
    ++vpid_counts[output.people(0).virtual_person_id()];
  }
  for (auto& [key, value] : vpid_counts) {
    value /= static_cast<double>(kEventIdNumber);
  }
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(vpid_counts,
              UnorderedElementsAre(Pair(10, DoubleNear(0.4, 0.02)),
                                   Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(LabelerTest, TestBuildFromNodesRootWithoutIndex) {
  // All nodes, except root node, have index set.
  // A model with
  // * 40% probability to assign virtual person id 10
  // * 60% probability to assign virtual person id 20
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 2
        name: "TestNode2"
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 3
        name: "TestNode3"
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestNode1"
        branch_node {
          branches { node_index: 2 chance: 0.4 }
          branches { node_index: 3 chance: 0.6 }
          random_seed: "TestBranchNodeSeed"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Labeler> labeler, Labeler::Build(nodes));

  absl::flat_hash_map<int64_t, double> vpid_counts;
  for (int event_id = 0; event_id < kEventIdNumber; ++event_id) {
    LabelerInput input;
    input.mutable_event_id()->set_id(std::to_string(event_id));
    LabelerOutput output;
    EXPECT_THAT(labeler->Label(input, output), IsOk());
    ++vpid_counts[output.people(0).virtual_person_id()];
  }
  for (auto& [key, value] : vpid_counts) {
    value /= static_cast<double>(kEventIdNumber);
  }
  // Absolute error more than 2% is very unlikely.
  EXPECT_THAT(vpid_counts,
              UnorderedElementsAre(Pair(10, DoubleNear(0.4, 0.02)),
                                   Pair(20, DoubleNear(0.6, 0.02))));
}

TEST(LabelerTest, TestBuildFromListWithSingleNode) {
  // A model with single node, which always assigns virtual person id 10.
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestNode1"
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Labeler> labeler, Labeler::Build(nodes));

  for (int event_id = 0; event_id < kEventIdNumber; ++event_id) {
    LabelerInput input;
    input.mutable_event_id()->set_id(std::to_string(event_id));
    LabelerOutput output;
    EXPECT_THAT(labeler->Label(input, output), IsOk());
    EXPECT_EQ(output.people(0).virtual_person_id(), 10);
  }
}

TEST(LabelerTest, TestBuildFromNodesNodeAfterRoot) {
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 3
        name: "TestNode3"
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestNode1"
        branch_node {
          branches { node_index: 3 chance: 1 }
          random_seed: "TestBranchNodeSeed"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestNode2"
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  EXPECT_THAT(Labeler::Build(nodes).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(LabelerTest, TestBuildFromNodesMultipleRoots) {
  // 2 trees exist:
  // 1 -> 3, 4
  // 2 -> 5
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 3
        name: "TestNode3"
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 4
        name: "TestNode4"
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 5
        name: "TestNode5"
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed3"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 1
        name: "TestNode1"
        branch_node {
          branches { node_index: 3 chance: 0.4 }
          branches { node_index: 4 chance: 0.6 }
          random_seed: "TestBranchNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 2
        name: "TestNode2"
        branch_node {
          branches { node_index: 5 chance: 1 }
          random_seed: "TestBranchNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  EXPECT_THAT(Labeler::Build(nodes).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(LabelerTest, TestBuildFromNodesNoRoot) {
  // Nodes 1 and 2 reference each other as child node. No root node can be
  // recognized.
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 1
        name: "TestNode1"
        branch_node {
          branches { node_index: 2 chance: 1 }
          random_seed: "TestBranchNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 2
        name: "TestNode2"
        branch_node {
          branches { node_index: 1 chance: 1 }
          random_seed: "TestBranchNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  EXPECT_THAT(Labeler::Build(nodes).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(LabelerTest, TestBuildFromNodesNoNodeForIndex) {
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 1
        name: "TestNode1"
        branch_node {
          branches { node_index: 2 chance: 1 }
          random_seed: "TestBranchNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  EXPECT_THAT(Labeler::Build(nodes).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(LabelerTest, TestBuildFromNodesDuplicatedIndexes) {
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 2
        name: "TestNode2"
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 2
        name: "TestNode3"
        population_node {
          pools { population_offset: 20 total_population: 1 }
          random_seed: "TestPopulationNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestNode1"
        branch_node {
          branches { node_index: 2 chance: 1 }
          random_seed: "TestBranchNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  EXPECT_THAT(Labeler::Build(nodes).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(LabelerTest, TestBuildFromNodesMultipleParents) {
  // This is invalid as node 3 is the child node of both node 1 and node 2.
  std::vector<CompiledNode> nodes;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 3
        name: "TestNode3"
        population_node {
          pools { population_offset: 10 total_population: 1 }
          random_seed: "TestPopulationNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        index: 2
        name: "TestNode2"
        branch_node {
          branches { node_index: 3 chance: 1 }
          random_seed: "TestBranchNodeSeed2"
        }
      )pb",
      &nodes.emplace_back()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestNode1"
        branch_node {
          branches { node_index: 2 chance: 0.5 }
          branches { node_index: 3 chance: 0.5 }
          random_seed: "TestBranchNodeSeed1"
        }
      )pb",
      &nodes.emplace_back()));
  EXPECT_THAT(Labeler::Build(nodes).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

}  // namespace
}  // namespace wfa_virtual_people
