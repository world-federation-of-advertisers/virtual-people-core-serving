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

package org.wfanet.virtualpeople.core.labeler

import com.google.protobuf.TextFormat
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import org.wfanet.virtualpeople.common.CompiledNode
import org.wfanet.virtualpeople.common.LabelerInput
import org.wfanet.virtualpeople.common.RankAssignment

@RunWith(JUnit4::class)
class RankedLabelingIntegrationTest {

  @Test
  fun `ranked events produce collision free VIDs`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "test-seed"
          ranked_size: 200
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())
    val vids = mutableSetOf<ULong>()

    for (rank in 0uL until 200uL) {
      val input =
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test")
              .setId(rank.toString())
          )
          .addRankAssignments(
            RankAssignment.newBuilder()
              .setPoolOffset(100)
              .setLocalRank(rank.toLong())
          )
          .build()

      val output = labeler.label(input)
      assertEquals(1, output.peopleCount)
      val vid = output.peopleList[0].virtualPersonId.toULong()
      // Ranked VIDs in [pool_offset, pool_offset + ranked_size)
      assertTrue(vid >= 100uL, "VID $vid below pool_offset")
      assertTrue(vid < 300uL, "VID $vid above ranked range")
      vids.add(vid)
    }

    // All 200 ranked events must produce 200 distinct VIDs.
    assertEquals(200, vids.size, "Collision detected in ranked VIDs")
  }

  @Test
  fun `unranked disjoint events get VIDs in unranked range`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "test-seed"
          ranked_size: 200
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    for (i in 0 until 50) {
      val input =
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test")
              .setId("unranked-$i")
          )
          .build()

      val output = labeler.label(input)
      assertEquals(1, output.peopleCount)
      val vid = output.peopleList[0].virtualPersonId.toULong()
      // DISJOINT: VIDs in [pool_offset + ranked_size, pool_offset + pool_size)
      assertTrue(vid >= 300uL, "VID $vid below unranked range")
      assertTrue(vid < 600uL, "VID $vid above pool range")
    }
  }

  @Test
  fun `unranked full pool events get VIDs in full range`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "test-seed"
          ranked_size: 200
          unranked_mode: FULL_POOL
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    for (i in 0 until 50) {
      val input =
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test")
              .setId("fullpool-$i")
          )
          .build()

      val output = labeler.label(input)
      assertEquals(1, output.peopleCount)
      val vid = output.peopleList[0].virtualPersonId.toULong()
      // FULL_POOL: VIDs in [pool_offset, pool_offset + pool_size)
      assertTrue(vid >= 100uL, "VID $vid below pool range")
      assertTrue(vid < 600uL, "VID $vid above pool range")
    }
  }

  @Test
  fun `ranked size zero behaves like hash based`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "HashOnlyNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 1000 }
          random_seed: "hash-only-seed"
          ranked_size: 0
          unranked_mode: FULL_POOL
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())
    for (i in 0 until 50) {
      val input =
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test")
              .setId(i.toString())
          )
          .build()

      val output = labeler.label(input)
      assertEquals(1, output.peopleCount)
      val vid = output.peopleList[0].virtualPersonId.toULong()
      assertTrue(vid >= 100uL)
      assertTrue(vid < 1100uL)
    }
  }
}
