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

package org.wfanet.virtualpeople.core.model

import com.google.common.hash.Hashing
import java.nio.charset.StandardCharsets
import org.wfanet.virtualpeople.common.CompiledNode
import org.wfanet.virtualpeople.common.LabelerEvent
import org.wfanet.virtualpeople.common.PersonLabelAttributes
import org.wfanet.virtualpeople.common.PoolAssignment
import org.wfanet.virtualpeople.common.QuantumLabel
import org.wfanet.virtualpeople.common.UnrankedMode
import org.wfanet.virtualpeople.common.VirtualPersonActivity
import org.wfanet.virtualpeople.core.model.utils.DistributedConsistentHashing
import org.wfanet.virtualpeople.core.model.utils.DistributionChoice
import org.wfanet.virtualpeople.core.model.utils.Feistel

/**
 * Implementation of CompiledNode with ranked_population_node set.
 *
 * Splits VID assignment into ranked (Feistel, collision-free) and unranked (hash-based) sub-ranges.
 */
internal class RankedPopulationNodeImpl
private constructor(
  nodeConfig: CompiledNode,
  private val randomSeed: String,
  private val rankedSize: ULong,
  private val unrankedMode: UnrankedMode,
  private val poolOffset: ULong,
  private val poolSize: ULong,
) : ModelNode(nodeConfig) {

  override fun apply(event: LabelerEvent.Builder) {
    // Pass-1 mode: emit pool identity and return without assigning a VID.
    if (event.poolIdentityMode) {
      event.addPoolAssignments(
        PoolAssignment.newBuilder()
          .setPoolOffset(poolOffset.toLong())
          .setPoolSize(poolSize.toLong())
          .setRankedSize(rankedSize.toLong())
      )
      return
    }

    if (event.virtualPersonActivitiesCount > 0) {
      error("virtual_person_activities should only be created in leaf nodes.")
    }

    val activity = VirtualPersonActivity.newBuilder()

    // Look up pre-computed rank from LabelerInput.rank_assignments.
    var hasRank = false
    var localRank = 0uL
    if (event.hasLabelerInput()) {
      val rankAssignment =
        event.labelerInput.rankAssignmentsList.firstOrNull {
          it.poolOffset.toULong() == poolOffset
        }
      if (rankAssignment != null) {
        localRank = rankAssignment.localRank.toULong()
        hasRank = true
      }
    }

    val virtualPersonId: ULong
    if (hasRank && localRank < rankedSize) {
      // RANKED path: Feistel bijection — zero collisions.
      virtualPersonId = poolOffset + Feistel.permute(localRank, rankedSize, randomSeed)
    } else {
      // UNRANKED path: hash-based (mode-dependent scope).
      val seed =
        Hashing.farmHashFingerprint64()
          .hashString("$randomSeed${event.actingFingerprint.toULong()}", StandardCharsets.UTF_8)
          .asLong()
          .toULong()

      virtualPersonId =
        if (unrankedMode == UnrankedMode.DISJOINT) {
          val unrankedSize = poolSize - rankedSize
          check(unrankedSize > 0uL) {
            "DISJOINT mode with ranked_size == pool_size leaves no unranked space."
          }
          poolOffset + rankedSize + (seed % unrankedSize)
        } else {
          // FULL_POOL: hash into the entire pool.
          poolOffset + (seed % poolSize)
        }
    }

    activity.virtualPersonId = virtualPersonId.toLong()

    // Collapse quantum labels from the event (same as PopulationNodeImpl).
    if (event.hasQuantumLabels()) {
      val seedSuffix = virtualPersonId.toString()
      event.quantumLabels.quantumLabelsList.forEach {
        collapseQuantumLabel(it, seedSuffix, activity.labelBuilder)
      }
    }
    // Merge classic label from the event.
    if (event.hasLabel()) {
      activity.labelBuilder.mergeFrom(event.label)
    }

    event.addVirtualPersonActivities(activity)
  }

  companion object {
    private fun collapseQuantumLabel(
      quantumLabel: QuantumLabel,
      seedSuffix: String,
      outputLabel: PersonLabelAttributes.Builder
    ) {
      if (quantumLabel.labelsCount == 0) {
        error("Empty quantum label.")
      }
      if (quantumLabel.labelsCount != quantumLabel.probabilitiesCount) {
        error("The sizes of labels and probabilities are different in quantum label. $quantumLabel")
      }
      val distribution =
        quantumLabel.probabilitiesList.mapIndexed { index, it -> DistributionChoice(index, it) }
      val hashing = DistributedConsistentHashing(distribution)
      val index = hashing.hash("quantum-label-collapse-${quantumLabel.seed}$seedSuffix")
      outputLabel.mergeFrom(quantumLabel.getLabels(index))
    }

    fun build(nodeConfig: CompiledNode): RankedPopulationNodeImpl {
      if (!nodeConfig.hasRankedPopulationNode()) {
        error("This is not a ranked population node.")
      }
      val config = nodeConfig.rankedPopulationNode

      require(config.poolsCount > 0) { "RankedPopulationNode must have at least one pool." }

      val poolOffset = config.poolsList[0].populationOffset.toULong()
      var poolSize = 0uL
      config.poolsList.forEach { poolSize += it.totalPopulation.toULong() }

      require(poolSize > 0uL) { "RankedPopulationNode total pool size must be > 0." }

      val rankedSize = config.rankedSize.toULong()
      require(rankedSize <= poolSize) {
        "ranked_size ($rankedSize) must not exceed pool_size ($poolSize)."
      }

      return RankedPopulationNodeImpl(
        nodeConfig,
        config.randomSeed,
        rankedSize,
        config.unrankedMode,
        poolOffset,
        poolSize,
      )
    }
  }
}
