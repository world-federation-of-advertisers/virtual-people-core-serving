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
import kotlin.test.assertFailsWith
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
            RankAssignment.newBuilder().setPoolOffset(100).setLocalRank(rank.toLong())
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
  fun `rank overflow falls back to unranked path`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "overflow-seed"
          ranked_size: 200
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    // local_rank=250 >= ranked_size=200 — should fall back to unranked DISJOINT range.
    val input =
      LabelerInput.newBuilder()
        .setEventId(
          org.wfanet.virtualpeople.common.EventId.newBuilder()
            .setPublisher("test")
            .setId("overflow-event")
        )
        .addRankAssignments(RankAssignment.newBuilder().setPoolOffset(100).setLocalRank(250))
        .build()

    val output = labeler.label(input)
    assertEquals(1, output.peopleCount)
    val vid = output.peopleList[0].virtualPersonId.toULong()
    // DISJOINT unranked range: [pool_offset + ranked_size, pool_offset + pool_size)
    assertTrue(vid >= 300uL, "Overflow VID $vid should be in unranked range")
    assertTrue(vid < 600uL, "Overflow VID $vid should be in unranked range")
  }

  @Test
  fun `boundary rank equals ranked size falls back to unranked`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "boundary-seed"
          ranked_size: 200
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    // local_rank=200 == ranked_size=200 — exactly at boundary, should fall back to unranked.
    val input =
      LabelerInput.newBuilder()
        .setEventId(
          org.wfanet.virtualpeople.common.EventId.newBuilder()
            .setPublisher("test")
            .setId("boundary-event")
        )
        .addRankAssignments(RankAssignment.newBuilder().setPoolOffset(100).setLocalRank(200))
        .build()

    val output = labeler.label(input)
    assertEquals(1, output.peopleCount)
    val vid = output.peopleList[0].virtualPersonId.toULong()
    // DISJOINT unranked range: [pool_offset + ranked_size, pool_offset + pool_size)
    assertTrue(vid >= 300uL, "Boundary VID $vid should be in unranked range")
    assertTrue(vid < 600uL, "Boundary VID $vid should be in unranked range")
  }

  @Test
  fun `multiple rank assignments resolves correct pool`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 500 total_population: 1000 }
          random_seed: "multi-rank-seed"
          ranked_size: 400
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    // Event carries rank assignments for multiple pools — only pool_offset=500 should match.
    val input =
      LabelerInput.newBuilder()
        .setEventId(
          org.wfanet.virtualpeople.common.EventId.newBuilder()
            .setPublisher("test")
            .setId("multi-rank-event")
        )
        .addRankAssignments(RankAssignment.newBuilder().setPoolOffset(100).setLocalRank(5))
        .addRankAssignments(RankAssignment.newBuilder().setPoolOffset(500).setLocalRank(10))
        .addRankAssignments(RankAssignment.newBuilder().setPoolOffset(9000).setLocalRank(99))
        .build()

    val output = labeler.label(input)
    assertEquals(1, output.peopleCount)
    val vid = output.peopleList[0].virtualPersonId.toULong()
    // Should use pool_offset=500, local_rank=10 → ranked path: [500, 900)
    assertTrue(vid >= 500uL, "VID $vid should be in ranked range for pool 500")
    assertTrue(vid < 900uL, "VID $vid should be in ranked range for pool 500")
  }

  @Test
  fun `ranked size zero produces same VIDs as PopulationNode`() {
    val rankedRoot = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "RankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "same-seed"
          ranked_size: 0
          unranked_mode: FULL_POOL
        }
      """,
      rankedRoot,
    )

    val populationRoot = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "PopNode"
        population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "same-seed"
        }
      """,
      populationRoot,
    )

    val rankedLabeler = Labeler.build(rankedRoot.build())
    val popLabeler = Labeler.build(populationRoot.build())

    for (i in 0 until 100) {
      val input =
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test")
              .setId("event-$i")
          )
          .build()
      val rankedVid = rankedLabeler.label(input).peopleList[0].virtualPersonId
      val popVid = popLabeler.label(input).peopleList[0].virtualPersonId
      assertEquals(popVid, rankedVid, "VID mismatch for event-$i")
    }
  }

  @Test
  fun `pass 1 emits pool assignment without VID`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "pass1-seed"
          ranked_size: 200
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    val input =
      LabelerInput.newBuilder()
        .setEventId(
          org.wfanet.virtualpeople.common.EventId.newBuilder()
            .setPublisher("test")
            .setId("pass1-event")
        )
        .build()

    val output = labeler.label(input, LabelingMode.POOL_IDENTITY)

    assertEquals(0, output.peopleCount, "Pass-1 should not assign VIDs")
    assertEquals(1, output.poolAssignmentsCount, "Pass-1 should emit pool assignment")
    assertEquals(100L, output.poolAssignmentsList[0].poolOffset)
    assertEquals(500L, output.poolAssignmentsList[0].poolSize)
    assertEquals(200L, output.poolAssignmentsList[0].rankedSize)
  }

  @Test
  fun `pass 1 population node throws`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "PopNode"
        population_node {
          pools { population_offset: 10 total_population: 100 }
          random_seed: "pop-seed"
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    val input =
      LabelerInput.newBuilder()
        .setEventId(
          org.wfanet.virtualpeople.common.EventId.newBuilder()
            .setPublisher("test")
            .setId("pop-event")
        )
        .build()

    // PopulationNode does not support pool-identity mode — should throw.
    assertFailsWith<IllegalStateException> { labeler.label(input, LabelingMode.POOL_IDENTITY) }
  }

  @Test
  fun `two pass collision free end to end`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 0 total_population: 300 }
          random_seed: "two-pass-seed"
          ranked_size: 100
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())
    val eventCount = 100

    // Pass 1: collect pool assignments.
    val inputs =
      (0 until eventCount).map { i ->
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test")
              .setId("event-$i")
          )
          .build()
      }

    inputs.forEach { input ->
      val output = labeler.label(input, LabelingMode.POOL_IDENTITY)
      assertEquals(1, output.poolAssignmentsCount)
      assertEquals(0, output.peopleCount)
    }

    // Pass 2: inject ranks and assign VIDs.
    val vids = mutableSetOf<ULong>()
    inputs.forEachIndexed { rank, input ->
      val rankedInput =
        input.toBuilder()
          .addRankAssignments(
            RankAssignment.newBuilder().setPoolOffset(0).setLocalRank(rank.toLong())
          )
          .build()

      val output = labeler.label(rankedInput)
      assertEquals(1, output.peopleCount)
      val vid = output.peopleList[0].virtualPersonId.toULong()
      assertTrue(vid < 100uL, "Ranked VID $vid should be in [0, 100)")
      vids.add(vid)
    }

    // All ranked events must produce unique VIDs.
    assertEquals(eventCount, vids.size, "Collision detected in two-pass flow")
  }

  @Test
  fun `rank for non-existent pool falls back to unranked`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "TestRankedNode"
        ranked_population_node {
          pools { population_offset: 100 total_population: 500 }
          random_seed: "wrong-pool-seed"
          ranked_size: 200
          unranked_mode: DISJOINT
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())

    // Rank assignment for pool_offset=9999 which doesn't match pool_offset=100.
    val input =
      LabelerInput.newBuilder()
        .setEventId(
          org.wfanet.virtualpeople.common.EventId.newBuilder()
            .setPublisher("test")
            .setId("wrong-pool-event")
        )
        .addRankAssignments(RankAssignment.newBuilder().setPoolOffset(9999).setLocalRank(5))
        .build()

    val output = labeler.label(input)
    assertEquals(1, output.peopleCount)
    val vid = output.peopleList[0].virtualPersonId.toULong()
    // No matching rank — falls back to DISJOINT unranked range.
    assertTrue(vid >= 300uL, "VID $vid should be in unranked range")
    assertTrue(vid < 600uL, "VID $vid should be in unranked range")
  }
}
