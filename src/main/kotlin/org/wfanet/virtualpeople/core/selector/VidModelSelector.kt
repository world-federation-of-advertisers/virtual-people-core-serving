// Copyright 2023 The Cross-Media Measurement Authors
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

package org.wfanet.virtualpeople.core.selector

import java.util.*
import kotlin.collections.ArrayList
import kotlin.math.abs
import org.wfanet.measurement.api.v2alpha.ModelLine
import org.wfanet.measurement.api.v2alpha.ModelRollout
import org.wfanet.virtualpeople.common.*
import org.wfanet.virtualpeople.core.common.getFingerprint64Long

const val CACHE_SIZE = 60
const val LOWER_BOUND_PERCENTAGE_ADOPTION = 0.0
const val UPPER_BOUND_PERCENTAGE_ADOPTION = 1.1

class VidModelSelector(private val modelLine: ModelLine, private val rollouts: List<ModelRollout>) {

  init {
    val modelLineId = modelLine.name.split("/").drop(5).take(1).toString()
    for (rollout in rollouts) {
      val modelRolloutId = rollout.name.split("/").drop(5).take(1).toString()
      require(modelLineId == modelRolloutId) {
        "ModelRollouts must be parented by the provided ModelLine."
      }
    }
  }

  /**
   * The adoption percentage of models is calculated each day. It consists of an array of triples
   * where each triple wraps the lower and upper percentage bound for a particular ModelRelease. All
   * the triples together cover the totality of events for a given day.
   *
   * E.g. [<0.0,0.5,ModelRelease1>,<0.5,1.1,ModelRelease2>] means that if the reducedEventId is
   * lower than 0.5, ModelRelease1 is returned, ModelRelease2 otherwise.
   */
  private val lruCache =
    Collections.synchronizedMap(
      LRUCache<Long, ArrayList<Triple<Double, Double, String>>>(CACHE_SIZE)
    )

  fun getModelRelease(labelerInput: LabelerInput): String? {
    val eventTimestampSec = labelerInput.timestampUsec / 1_000_000L
    if (
      eventTimestampSec >= modelLine.activeStartTime.seconds &&
        eventTimestampSec < modelLine.activeEndTime.seconds
    ) {
      val eventFingerprint = getEventFingerprint(labelerInput)
      val reducedEventId = abs(eventFingerprint.toDouble() / Long.MAX_VALUE)
      val eventDay: Long = eventTimestampSec / 86_400L
      val ranges = readFromCache(eventDay)
      for (triple in ranges) {
        if (reducedEventId < triple.second) {
          return triple.third
        }
      }
    }
    return null
  }

  private fun readFromCache(eventDay: Long): ArrayList<Triple<Double, Double, String>> {
    if (lruCache.containsKey(eventDay)) {
      return lruCache[eventDay]!!
    } else {
      val ranges = calculateRanges(eventDay)
      lruCache[eventDay] = ranges
      return ranges
    }
  }

  /**
   * A single triple with range 0.0..1.1 is returned if there is a single active ModelRollout. In
   * case there are multiple active ModelRollout(s), the function iterates through an ordered list
   * of active ModelRollout(s). The list is ordered by rollout_period_start_time, from the most
   * recent to the oldest.
   *
   * The adoption percentage of each ModelRollout is calculated as follows: ((EVENT_DAY
   * - ROLLOUT_START_DAY) / (ROLLOUT_END_DAY - ROLLOUT_START_DAY) - (1 *
   *   percentage_taken_by_previous_rollout))
   *
   * In case a ModelRollout has the `rollout_freeze_time` set and the event day is greater than
   * rollout_freeze_time, the EVENT_DAY in the above formula is replaced by `rollout_freeze_time` to
   * ensure that the rollout stops its expansion. ((ROLLOUT_FREEZE_TIME - ROLLOUT_START_DAY) /
   * (ROLLOUT_END_DAY - ROLLOUT_START_DAY) - (1 * percentage_taken_by_previous_rollout))
   */
  private fun calculateRanges(eventDay: Long): ArrayList<Triple<Double, Double, String>> {
    val activeRollouts = retrieveActiveRollouts(eventDay)
    if (activeRollouts.size == 0) {
      return arrayListOf()
    }
    val ranges = arrayListOf<Triple<Double, Double, String>>()
    if (activeRollouts.size == 1) {
      ranges.add(
        Triple(
          LOWER_BOUND_PERCENTAGE_ADOPTION,
          UPPER_BOUND_PERCENTAGE_ADOPTION,
          activeRollouts.elementAt(0).modelRelease
        )
      )
    } else {
      ranges.add(
        Triple(
          LOWER_BOUND_PERCENTAGE_ADOPTION,
          calculatePercentageAdoption(eventDay, activeRollouts.elementAt(0)),
          activeRollouts.elementAt(0).modelRelease
        )
      )
      var taken = ranges.elementAt(0).second
      for (i in 1 until activeRollouts.size) {
        if (i < activeRollouts.size - 1) {
          val percentageAdoption =
            calculatePercentageAdoption(eventDay, activeRollouts.elementAt(i))
          ranges.add(
            Triple(
              ranges[i - 1].second,
              (percentageAdoption * (1 - taken)) + ranges[i - 1].second,
              activeRollouts.elementAt(i).modelRelease
            )
          )
        } else {
          ranges.add(
            Triple(
              ranges[i - 1].second,
              UPPER_BOUND_PERCENTAGE_ADOPTION,
              activeRollouts.elementAt(i).modelRelease
            )
          )
        }
        taken = ranges[i].second
      }
    }
    return ranges
  }

  private fun calculatePercentageAdoption(eventDay: Long, modelRollout: ModelRollout): Double {
    val modelRolloutFreezeTimeDay =
      if (modelRollout.hasRolloutFreezeTime()) {
        modelRollout.rolloutFreezeTime.seconds / 86_400L
      } else {
        Long.MAX_VALUE
      }

    val rolloutPeriodStartDay = (modelRollout.rolloutPeriod.startTime.seconds / 86_400L).toDouble()
    val rolloutPeriodEndDay = (modelRollout.rolloutPeriod.endTime.seconds / 86_400L).toDouble()

    if (eventDay >= modelRolloutFreezeTimeDay) {
      val percentage: Double =
        (modelRolloutFreezeTimeDay - rolloutPeriodStartDay) /
          (rolloutPeriodEndDay - rolloutPeriodStartDay)
      return percentage
    } else {
      val percentage: Double =
        (eventDay - rolloutPeriodStartDay) / (rolloutPeriodEndDay - rolloutPeriodStartDay)
      return percentage
    }
  }

  /**
   * Iterates through all available ModelRollout(s) ordered by rollout_period_start_time from the
   * most recent to the oldest. The function keeps adding ModelRollout(s) to the `activeRollouts`
   * array unless one of the following conditions is met:
   * 1. eventDay >= rolloutPeriodEndTime && !rollout.hasRolloutFreezeTime()
   * 2. rolloutPeriodStartTime == rolloutPeriodEndTime. In this case there is no gradual rollout
   */
  private fun retrieveActiveRollouts(eventDay: Long): List<ModelRollout> {
    if (rollouts.isEmpty()) {
      return rollouts
    }
    val sortedRollouts = rollouts.sortedBy { it.rolloutPeriod.startTime.seconds }
    val firstRolloutPeriodStartDay =
      sortedRollouts.elementAt(0).rolloutPeriod.startTime.seconds / 86_400L
    if (eventDay < firstRolloutPeriodStartDay) {
      return arrayListOf()
    }
    val activeRollouts = arrayListOf<ModelRollout>()
    for (i in sortedRollouts.size - 1 downTo 0) {
      val rolloutPeriodEndDay = sortedRollouts.elementAt(i).rolloutPeriod.endTime.seconds / 86_400L
      if (eventDay >= rolloutPeriodEndDay) {
        val rollout = sortedRollouts.elementAt(i)
        activeRollouts.add(rollout)
        if (!rollout.hasRolloutFreezeTime()) {
          // Stop only if there is no Freeze time set, otherwise we take one more rollout, if
          // present
          break
        }
        continue
      }
      val rolloutPeriodStartDay =
        sortedRollouts.elementAt(i).rolloutPeriod.startTime.seconds / 86_400L
      if (eventDay >= rolloutPeriodStartDay) {
        activeRollouts.add(sortedRollouts.elementAt(i))
        if (
          sortedRollouts.elementAt(i).rolloutPeriod.startTime.seconds ==
            sortedRollouts.elementAt(i).rolloutPeriod.endTime.seconds
        ) {
          // No gradual rollout. The rollout immediately labels 100% of events
          break
        }
      }
    }
    return activeRollouts
  }

  private fun getEventFingerprint(labelerInput: LabelerInput): Long {
    if (labelerInput.hasProfileInfo()) {
      val profileInfo = labelerInput.profileInfo
      if (profileInfo.hasEmailUserInfo() && profileInfo.emailUserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.emailUserInfo.userId)
      }
      if (profileInfo.hasPhoneUserInfo() && profileInfo.phoneUserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.phoneUserInfo.userId)
      }
      if (profileInfo.hasLoggedInIdUserInfo() && profileInfo.loggedInIdUserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.loggedInIdUserInfo.userId)
      }
      if (profileInfo.hasLoggedOutIdUserInfo() && profileInfo.loggedOutIdUserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.loggedOutIdUserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace1UserInfo() &&
          profileInfo.proprietaryIdSpace1UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace1UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace2UserInfo() &&
          profileInfo.proprietaryIdSpace2UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace2UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace3UserInfo() &&
          profileInfo.proprietaryIdSpace3UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace3UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace4UserInfo() &&
          profileInfo.proprietaryIdSpace4UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace4UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace5UserInfo() &&
          profileInfo.proprietaryIdSpace5UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace5UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace6UserInfo() &&
          profileInfo.proprietaryIdSpace6UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace6UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace7UserInfo() &&
          profileInfo.proprietaryIdSpace7UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace7UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace8UserInfo() &&
          profileInfo.proprietaryIdSpace8UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace8UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace9UserInfo() &&
          profileInfo.proprietaryIdSpace9UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace9UserInfo.userId)
      }
      if (
        profileInfo.hasProprietaryIdSpace10UserInfo() &&
          profileInfo.proprietaryIdSpace10UserInfo.hasUserId()
      ) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace10UserInfo.userId)
      }
    } else if (labelerInput.hasEventId() && labelerInput.eventId.hasId()) {
      return getFingerprint64Long(labelerInput.eventId.id)
    }
    return (0..Long.MAX_VALUE).random()
  }
}
