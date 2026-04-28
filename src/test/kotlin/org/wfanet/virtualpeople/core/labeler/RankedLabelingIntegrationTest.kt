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
  fun `two pass collision free`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "Root"
        branch_node {
          branches {
            node {
              name: "DisjointLeaf"
              ranked_population_node {
                pools { population_offset: 1000 total_population: 500 }
                random_seed: "disjoint-seed"
                ranked_size: 200
                unranked_mode: DISJOINT
              }
            }
            chance: 0.5
          }
          branches {
            node {
              name: "FullPoolLeaf"
              ranked_population_node {
                pools { population_offset: 5000 total_population: 500 }
                random_seed: "fullpool-seed"
                ranked_size: 200
                unranked_mode: FULL_POOL
              }
            }
            chance: 0.5
          }
          random_seed: "branch-seed"
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())
    val eventCount = 200

    // === Pass 1: collect pool assignments ===
    data class EventInfo(
      val input: LabelerInput,
      val poolOffset: ULong,
      val poolSize: ULong,
      val rankedSize: ULong,
    )

    val events = mutableListOf<EventInfo>()
    val poolRankCounter = mutableMapOf<ULong, ULong>()

    for (i in 0 until eventCount) {
      val input =
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test-pub")
              .setId(i.toString())
          )
          .build()

      val output = labeler.label(input, LabelingMode.POOL_IDENTITY)

      assertEquals(1, output.poolAssignmentsCount, "Event $i should have 1 pool assignment")
      assertEquals(0, output.peopleCount, "Event $i should have no people in pass-1")

      val pa = output.poolAssignmentsList[0]
      val offset = pa.poolOffset.toULong()
      events.add(EventInfo(input, offset, pa.poolSize.toULong(), pa.rankedSize.toULong()))
      poolRankCounter[offset] = (poolRankCounter[offset] ?: 0uL) + 1uL
    }

    assertTrue(poolRankCounter.containsKey(1000uL), "No events routed to DISJOINT pool")
    assertTrue(poolRankCounter.containsKey(5000uL), "No events routed to FULL_POOL pool")

    // === Simulate ranking ===
    val poolNextRank = mutableMapOf<ULong, ULong>()
    val rankedInputs =
      events.map { ev ->
        val rank = poolNextRank.getOrDefault(ev.poolOffset, 0uL)
        poolNextRank[ev.poolOffset] = rank + 1uL
        ev.input
          .toBuilder()
          .addRankAssignments(
            RankAssignment.newBuilder()
              .setPoolOffset(ev.poolOffset.toLong())
              .setLocalRank(rank.toLong())
          )
          .build() to ev
      }

    // === Pass 2: assign VIDs ===
    val rankedVids = mutableMapOf<ULong, MutableSet<ULong>>()

    for ((input, ev) in rankedInputs) {
      val output = labeler.label(input)
      assertEquals(1, output.peopleCount)
      assertTrue(output.peopleList[0].hasVirtualPersonId())
      val vid = output.peopleList[0].virtualPersonId.toULong()

      val localRank = input.rankAssignmentsList.first { it.poolOffset.toULong() == ev.poolOffset }.localRank.toULong()

      if (localRank < ev.rankedSize) {
        assertTrue(vid >= ev.poolOffset, "Ranked VID below pool_offset")
        assertTrue(vid < ev.poolOffset + ev.rankedSize, "Ranked VID above ranked range")
        rankedVids.getOrPut(ev.poolOffset) { mutableSetOf() }.add(vid)
      } else {
        if (ev.poolOffset == 1000uL) {
          assertTrue(vid >= ev.poolOffset + ev.rankedSize)
          assertTrue(vid < ev.poolOffset + ev.poolSize)
        } else {
          assertTrue(vid >= ev.poolOffset)
          assertTrue(vid < ev.poolOffset + ev.poolSize)
        }
      }
    }

    // Verify collision-free
    for ((poolOffset, vids) in rankedVids) {
      val eventsInPool = poolRankCounter[poolOffset]!!
      val expectedRanked = minOf(eventsInPool, 200uL)
      assertEquals(
        expectedRanked.toInt(),
        vids.size,
        "Collision in ranked VIDs for pool $poolOffset",
      )
    }
  }

  @Test
  fun `mixed model pass 1`() {
    val root = CompiledNode.newBuilder()
    TextFormat.merge(
      """
        name: "MixedRoot"
        branch_node {
          branches {
            node {
              name: "ClassicLeaf"
              population_node {
                pools { population_offset: 10 total_population: 100 }
                random_seed: "classic-seed"
              }
            }
            chance: 0.5
          }
          branches {
            node {
              name: "RankedLeaf"
              ranked_population_node {
                pools { population_offset: 2000 total_population: 500 }
                random_seed: "ranked-seed"
                ranked_size: 300
                unranked_mode: DISJOINT
              }
            }
            chance: 0.5
          }
          random_seed: "mixed-branch-seed"
        }
      """,
      root,
    )

    val labeler = Labeler.build(root.build())
    var poolAssignmentCount = 0
    var peopleCount = 0

    for (i in 0 until 200) {
      val input =
        LabelerInput.newBuilder()
          .setEventId(
            org.wfanet.virtualpeople.common.EventId.newBuilder()
              .setPublisher("test")
              .setId(i.toString())
          )
          .build()

      val output = labeler.label(input, LabelingMode.POOL_IDENTITY)
      poolAssignmentCount += output.poolAssignmentsCount
      peopleCount += output.peopleCount
    }

    assertTrue(poolAssignmentCount > 0, "No events routed to RankedPopulationNode")
    assertTrue(peopleCount > 0, "No events routed to PopulationNode")
  }

  @Test
  fun `ranked size zero is hash based`() {
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
