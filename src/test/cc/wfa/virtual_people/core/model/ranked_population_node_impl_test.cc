// Copyright 2026 The Cross-Media Measurement Authors
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

#include <cstdint>
#include <unordered_set>

#include "common_cpp/testing/status_macros.h"
#include "common_cpp/testing/status_matchers.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "wfa/virtual_people/common/event.pb.h"
#include "wfa/virtual_people/common/label.pb.h"
#include "wfa/virtual_people/common/model.pb.h"
#include "wfa/virtual_people/core/model/model_node.h"

namespace wfa_virtual_people {
namespace {

using ::testing::Ge;
using ::testing::Lt;
using ::wfa::IsOk;
using ::wfa::StatusIs;

TEST(RankedPopulationNodeImplTest, RankedPathAssignsVidInRankedRange) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "test-seed"
          ranked_size: 500
          unranked_mode: DISJOINT
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

  LabelerEvent event;
  event.set_acting_fingerprint(42);
  // Set a rank assignment that matches the pool offset.
  auto* ra = event.mutable_labeler_input()->add_rank_assignments();
  ra->set_pool_offset(100);
  ra->set_local_rank(7);

  EXPECT_THAT(node->Apply(event), IsOk());
  uint64_t vid = event.virtual_person_activities(0).virtual_person_id();
  // Ranked VIDs must be in [pool_offset, pool_offset + ranked_size).
  EXPECT_GE(vid, 100);
  EXPECT_LT(vid, 600);
}

TEST(RankedPopulationNodeImplTest, UnrankedDisjointAssignsVidInUnrankedRange) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "test-seed"
          ranked_size: 500
          unranked_mode: DISJOINT
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

  // No rank assignment — falls through to unranked path.
  LabelerEvent event;
  event.set_acting_fingerprint(42);

  EXPECT_THAT(node->Apply(event), IsOk());
  uint64_t vid = event.virtual_person_activities(0).virtual_person_id();
  // DISJOINT: VIDs in [pool_offset + ranked_size, pool_offset + pool_size).
  EXPECT_GE(vid, 600);
  EXPECT_LT(vid, 1100);
}

TEST(RankedPopulationNodeImplTest, UnrankedFullPoolAssignsVidInFullRange) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "test-seed"
          ranked_size: 500
          unranked_mode: FULL_POOL
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

  LabelerEvent event;
  event.set_acting_fingerprint(42);

  EXPECT_THAT(node->Apply(event), IsOk());
  uint64_t vid = event.virtual_person_activities(0).virtual_person_id();
  // FULL_POOL: VIDs in [pool_offset, pool_offset + pool_size).
  EXPECT_GE(vid, 100);
  EXPECT_LT(vid, 1100);
}

TEST(RankedPopulationNodeImplTest, RankedSizeZeroBehavesLikeHashBased) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "test-seed"
          ranked_size: 0
          unranked_mode: FULL_POOL
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

  LabelerEvent event;
  event.set_acting_fingerprint(42);

  EXPECT_THAT(node->Apply(event), IsOk());
  uint64_t vid = event.virtual_person_activities(0).virtual_person_id();
  EXPECT_GE(vid, 100);
  EXPECT_LT(vid, 1100);
}

TEST(RankedPopulationNodeImplTest, CollisionFreeRankedEvents) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 0 total_population: 200 }
          random_seed: "collision-test"
          ranked_size: 100
          unranked_mode: DISJOINT
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

  std::unordered_set<uint64_t> vids;
  for (uint64_t rank = 0; rank < 100; ++rank) {
    LabelerEvent event;
    event.set_acting_fingerprint(rank);
    auto* ra = event.mutable_labeler_input()->add_rank_assignments();
    ra->set_pool_offset(0);
    ra->set_local_rank(rank);

    EXPECT_THAT(node->Apply(event), IsOk());
    uint64_t vid = event.virtual_person_activities(0).virtual_person_id();
    EXPECT_GE(vid, 0);
    EXPECT_LT(vid, 100);
    vids.insert(vid);
  }
  // All 100 ranked events must produce 100 distinct VIDs.
  EXPECT_EQ(vids.size(), 100);
}

TEST(RankedPopulationNodeImplTest, Determinism) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "determinism-seed"
          ranked_size: 500
          unranked_mode: DISJOINT
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node1,
                       ModelNode::Build(config));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node2,
                       ModelNode::Build(config));

  for (int fp = 0; fp < 100; ++fp) {
    LabelerEvent event1;
    event1.set_acting_fingerprint(fp);
    LabelerEvent event2;
    event2.set_acting_fingerprint(fp);

    EXPECT_THAT(node1->Apply(event1), IsOk());
    EXPECT_THAT(node2->Apply(event2), IsOk());
    EXPECT_EQ(event1.virtual_person_activities(0).virtual_person_id(),
              event2.virtual_person_activities(0).virtual_person_id());
  }
}

TEST(RankedPopulationNodeImplTest, ExistingVirtualPersonError) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "test-seed"
          ranked_size: 500
          unranked_mode: DISJOINT
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

  LabelerEvent event;
  event.add_virtual_person_activities()->set_virtual_person_id(42);
  event.set_acting_fingerprint(42);

  EXPECT_THAT(node->Apply(event),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(RankedPopulationNodeImplTest, InvalidEmptyPools) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          random_seed: "test-seed"
          ranked_size: 0
          unranked_mode: FULL_POOL
        }
      )pb",
      &config));

  EXPECT_THAT(ModelNode::Build(config).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(RankedPopulationNodeImplTest, InvalidRankedSizeExceedsPoolSize) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 50 }
          random_seed: "test-seed"
          ranked_size: 100
          unranked_mode: DISJOINT
        }
      )pb",
      &config));

  EXPECT_THAT(ModelNode::Build(config).status(),
              StatusIs(absl::StatusCode::kInvalidArgument, ""));
}

TEST(RankedPopulationNodeImplTest, ApplyWithClassicLabel) {
  CompiledNode config;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "TestRankedNode"
        index: 1
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "test-seed"
          ranked_size: 500
          unranked_mode: DISJOINT
        }
      )pb",
      &config));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ModelNode> node,
                       ModelNode::Build(config));

  LabelerEvent event;
  event.set_acting_fingerprint(42);
  event.mutable_label()->mutable_demo()->set_gender(GENDER_FEMALE);

  EXPECT_THAT(node->Apply(event), IsOk());
  EXPECT_TRUE(event.virtual_person_activities(0).has_label());
  EXPECT_EQ(event.virtual_person_activities(0).label().demo().gender(),
            GENDER_FEMALE);
}

}  // namespace
}  // namespace wfa_virtual_people
