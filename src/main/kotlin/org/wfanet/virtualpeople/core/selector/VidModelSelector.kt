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

import java.nio.ByteOrder
import kotlin.collections.ArrayList
import kotlin.math.abs
import org.wfanet.measurement.api.v2alpha.ModelLine
import org.wfanet.measurement.api.v2alpha.ModelRollout
import org.wfanet.measurement.common.toLong
import org.wfanet.virtualpeople.common.LabelerInput
import org.wfanet.virtualpeople.core.common.Hashing

const val CACHE_SIZE = 60
const val UPPER_BOUND_PERCENTAGE_ADOPTION = 1.1

class VidModelSelector(private val modelLine: ModelLine, private val rollouts: List<ModelRollout>) {

  init {
    /**
     * Drop the first 5 elements of the List<String> returned by split() method and take the sixth.
     * ModelLine name has the following format:
     * "modelProviders/{model_provider}/modelSuites/{model_suite}/modelLines/{model_line}"
     */
    val modelLineId = modelLine.name.split("/").drop(5).take(1).toString()
    for (rollout in rollouts) {
      val modelRolloutModelLineId = rollout.name.split("/").drop(5).take(1).toString()
      require(modelLineId == modelRolloutModelLineId) {
        "ModelRollouts must be parented by the provided ModelLine."
      }
    }
  }

  /**
   * The adoption percentage of models is calculated each day. It consists of an array of Pairs
   * where each Pair wraps the percentage of adoption of a particular ModelRelease and the
   * ModelRelease itself.
   *
   * Elements in the ArrayList are sorted by ModelRollout rollout start time from the oldest to the
   * most recent.
   */
  private val lruCache: LruCache<Long, ArrayList<Pair<Double, String>>> = LruCache(CACHE_SIZE)

  fun getModelRelease(labelerInput: LabelerInput): String? {
    val eventTimestampSec = labelerInput.timestampUsec / 1_000_000L
    if (
      eventTimestampSec >= modelLine.activeStartTime.seconds &&
        eventTimestampSec < modelLine.activeEndTime.seconds
    ) {
      val eventDay: Long = eventTimestampSec / 86_400L
      val modelAdoptionPercentages = readFromCache(eventDay)

      var selectedModelRelease =
        if (!modelAdoptionPercentages.isEmpty()) {
          modelAdoptionPercentages.get(0).second
        } else {
          null
        }
      for (percentage in modelAdoptionPercentages) {
        val eventFingerprint =
          Hashing.hashFingerprint64(
              buildString {
                append(percentage.second)
                append(getEventId(labelerInput))
              }
            )
            .toLong(ByteOrder.LITTLE_ENDIAN)
        val reducedEventId = abs(eventFingerprint.toDouble() / Long.MAX_VALUE)
        if (reducedEventId < percentage.first) {
          selectedModelRelease = percentage.second
        }
      }
      return selectedModelRelease
    }
    return null
  }

  /**
   * Access to the cache is synchronized to prevent multiple threads calculating percentages in case
   * of cache miss.
   */
  private fun readFromCache(eventDay: Long): ArrayList<Pair<Double, String>> {
    synchronized(this) {
      if (lruCache.containsKey(eventDay)) {
        return lruCache[eventDay]!!
      } else {
        val percentages = calculatePercentages(eventDay)
        lruCache[eventDay] = percentages
        return percentages
      }
    }
  }

  /**
   * Return a list of Pairs. Each pair wraps the percentage of adoption of a particular ModelRelease
   * and the ModelRelease itself. The list is sorted by rollout_period_start_time.
   *
   * The adoption percentage of each ModelRollout is calculated as follows: (EVENT_DAY
   * - ROLLOUT_START_DAY) / (ROLLOUT_END_DAY - ROLLOUT_START_DAY)
   *
   * In case a ModelRollout has the `rollout_freeze_time` set and the event day is greater than
   * rollout_freeze_time, the EVENT_DAY in the above formula is replaced by `rollout_freeze_time` to
   * ensure that the rollout stops its expansion: (ROLLOUT_FREEZE_TIME - ROLLOUT_START_DAY) /
   * (ROLLOUT_END_DAY - ROLLOUT_START_DAY)
   */
  private fun calculatePercentages(eventDay: Long): ArrayList<Pair<Double, String>> {
    val activeRollouts = retrieveActiveRollouts(eventDay)
    val percentages = arrayListOf<Pair<Double, String>>()
    for (i in 0 until activeRollouts.size) {
      percentages.add(
        Pair(
          calculatePercentageAdoption(eventDay, activeRollouts.elementAt(i)),
          activeRollouts.elementAt(i).modelRelease
        )
      )
    }
    return percentages
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

    if (rolloutPeriodStartDay == rolloutPeriodEndDay) {
      return UPPER_BOUND_PERCENTAGE_ADOPTION
    } else if (eventDay >= modelRolloutFreezeTimeDay) {
      return (modelRolloutFreezeTimeDay - rolloutPeriodStartDay) /
        (rolloutPeriodEndDay - rolloutPeriodStartDay)
    } else {
      return (eventDay - rolloutPeriodStartDay) / (rolloutPeriodEndDay - rolloutPeriodStartDay)
    }
  }

  /**
   * Iterates through all available ModelRollout(s) sorted by rollout_period_start_time from the
   * most recent to the oldest. The function keeps adding ModelRollout(s) to the `activeRollouts`
   * array until the following condition is met: eventDay >= rolloutPeriodEndTime &&
   * !rollout.hasRolloutFreezeTime()
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
          // Stop only if there is no Freeze time set, otherwise other rollouts are taken until one
          // without rollout_freeze_time is found or no other rollouts are available
          break
        }
        continue
      }
      val rolloutPeriodStartDay =
        sortedRollouts.elementAt(i).rolloutPeriod.startTime.seconds / 86_400L
      if (eventDay >= rolloutPeriodStartDay) {
        activeRollouts.add(sortedRollouts.elementAt(i))
      }
    }
    activeRollouts.reverse()
    return activeRollouts
  }

  private fun getEventId(labelerInput: LabelerInput): String {
    if (labelerInput.hasProfileInfo()) {
      val profileInfo = labelerInput.profileInfo
      if (profileInfo.hasEmailUserInfo() && profileInfo.emailUserInfo.hasUserId()) {
        return profileInfo.emailUserInfo.userId
      }
      if (profileInfo.hasPhoneUserInfo() && profileInfo.phoneUserInfo.hasUserId()) {
        return profileInfo.phoneUserInfo.userId
      }
      if (profileInfo.hasLoggedInIdUserInfo() && profileInfo.loggedInIdUserInfo.hasUserId()) {
        return profileInfo.loggedInIdUserInfo.userId
      }
      if (profileInfo.hasLoggedOutIdUserInfo() && profileInfo.loggedOutIdUserInfo.hasUserId()) {
        return profileInfo.loggedOutIdUserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace1UserInfo() &&
          profileInfo.proprietaryIdSpace1UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace1UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace2UserInfo() &&
          profileInfo.proprietaryIdSpace2UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace2UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace3UserInfo() &&
          profileInfo.proprietaryIdSpace3UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace3UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace4UserInfo() &&
          profileInfo.proprietaryIdSpace4UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace4UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace5UserInfo() &&
          profileInfo.proprietaryIdSpace5UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace5UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace6UserInfo() &&
          profileInfo.proprietaryIdSpace6UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace6UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace7UserInfo() &&
          profileInfo.proprietaryIdSpace7UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace7UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace8UserInfo() &&
          profileInfo.proprietaryIdSpace8UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace8UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace9UserInfo() &&
          profileInfo.proprietaryIdSpace9UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace9UserInfo.userId
      }
      if (
        profileInfo.hasProprietaryIdSpace10UserInfo() &&
          profileInfo.proprietaryIdSpace10UserInfo.hasUserId()
      ) {
        return profileInfo.proprietaryIdSpace10UserInfo.userId
      }
    } else if (labelerInput.hasEventId() && labelerInput.eventId.hasId()) {
      return labelerInput.eventId.id
    }
    error("No user_id available in the LabelerInput")
  }
}
