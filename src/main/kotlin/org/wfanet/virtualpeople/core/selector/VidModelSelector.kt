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

import java.lang.IllegalArgumentException
import kotlin.math.abs
import org.wfanet.measurement.api.v2alpha.ModelLine
import org.wfanet.measurement.api.v2alpha.ModelRollout
import org.wfanet.virtualpeople.common.*
import org.wfanet.virtualpeople.core.common.getFingerprint64Long

const val CACHE_SIZE = 60

class VidModelSelector(private val modelLine: ModelLine, private val rollouts: List<ModelRollout>) {

  init {
    val modelLineId = modelLine.name.split("/").drop(5).take(1).toString()
      for (rollout in rollouts) {
        val modelRolloutId = rollout.name.split("/").drop(5).take(1).toString()
        require(modelLineId == modelRolloutId) { "ModelRollouts must be parented by the provided ModelLine." }
      }
  }

  private val lruCache: LRUCache<Long, ArrayList<Triple<Double, Double, String>>> = LRUCache(CACHE_SIZE)

  fun getModelRelease(labelerInput: LabelerInput): String? {
    val eventTimestampSec = labelerInput.timestampUsec / 1_000_000L
    if (eventTimestampSec >= modelLine.activeStartTime.seconds && eventTimestampSec < modelLine.activeEndTime.seconds) {
      val eventFingerprint = getEventFingerprint(labelerInput)
      val reducedEventId = abs(eventFingerprint.toDouble() / Long.MAX_VALUE)
      println("REduced event id: ${reducedEventId}")
      val eventDay: Long = eventTimestampSec / 86_400L
      val ranges = readFromCache(eventDay)
      for(triple in ranges) {
        println(triple)
      }
      for(triple in ranges) {
        if (reducedEventId < triple.second) {
          return triple.third
        }
      }
    }
    return null
  }

  private fun readFromCache(eventDay: Long): ArrayList<Triple<Double, Double, String>> {
    synchronized(this) {
      if (lruCache.contains(eventDay)) {
        return lruCache[eventDay]!!
      } else {
        val ranges = calculateRanges(eventDay)
        lruCache[eventDay] = ranges
        return ranges
      }
    }
  }

  private fun calculateRanges(eventDay: Long): ArrayList<Triple<Double, Double, String>> {
    val activeRollouts = retrieveActiveRollouts(eventDay)
    if (activeRollouts.size == 0) {
      return arrayListOf()
    }
    val ranges = arrayListOf<Triple<Double, Double, String>>()
    if (activeRollouts.size == 1) {
      ranges.add(Triple(0.0,1.1,activeRollouts.elementAt(0).modelRelease))
    } else {
      ranges.add(Triple(0.0, calculatePercentageAdoption(eventDay, activeRollouts.elementAt(0)), activeRollouts.elementAt(0).modelRelease))
      var taken = ranges.elementAt(0).second
      for (i in 1 until activeRollouts.size) {
        if (i < activeRollouts.size - 1) {
          val percentageAdoption = calculatePercentageAdoption(eventDay, activeRollouts.elementAt(i))
          ranges.add(Triple(ranges[i-1].second, (percentageAdoption * (1 - taken)) + ranges[i-1].second, activeRollouts.elementAt(i).modelRelease))
        } else {
          ranges.add(Triple(ranges[i-1].second, 1.1, activeRollouts.elementAt(i).modelRelease))
        }
        taken = ranges[i].second
      }
    }
    return ranges
  }

  private fun calculatePercentageAdoption(eventDay: Long, modelRollout: ModelRollout): Double {
    val modelRolloutFreezeTimeDay = if (modelRollout.hasRolloutFreezeTime()) {
      modelRollout.rolloutFreezeTime.seconds / 86_400L
    } else {
      Long.MAX_VALUE
    }

    val rolloutPeriodStartDay =
      (modelRollout.rolloutPeriod.startTime.seconds / 86_400L).toDouble()
    val rolloutPeriodEndDay =
      (modelRollout.rolloutPeriod.endTime.seconds / 86_400L).toDouble()

    if (eventDay >= modelRolloutFreezeTimeDay) {
      val percentage: Double =
        (modelRolloutFreezeTimeDay - rolloutPeriodStartDay) / (rolloutPeriodEndDay - rolloutPeriodStartDay)
      return percentage
    } else {
      val percentage: Double =
        (eventDay - rolloutPeriodStartDay) / (rolloutPeriodEndDay - rolloutPeriodStartDay)
      return percentage
    }
  }

  private fun retrieveActiveRollouts(eventDay: Long): List<ModelRollout> {
    if (rollouts.isEmpty()) {
      return rollouts
    }
    val sortedRollouts = rollouts.sortedBy { it.rolloutPeriod.startTime.seconds }
    val firstRolloutPeriodStartDay = sortedRollouts.elementAt(0).rolloutPeriod.startTime.seconds / 86_400L
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
          // Stop only if there is no Freeze time set, otherwise we take one more rollout, if present
          break
        }
        continue
      }
      val rolloutPeriodStartDay = sortedRollouts.elementAt(i).rolloutPeriod.startTime.seconds / 86_400L
      if (eventDay >= rolloutPeriodStartDay) {
        activeRollouts.add(sortedRollouts.elementAt(i))
        if (sortedRollouts.elementAt(i).rolloutPeriod.startTime.seconds == sortedRollouts.elementAt(i).rolloutPeriod.endTime.seconds) {
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
      if (profileInfo.hasProprietaryIdSpace1UserInfo() && profileInfo.proprietaryIdSpace1UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace1UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace2UserInfo() && profileInfo.proprietaryIdSpace2UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace2UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace3UserInfo() && profileInfo.proprietaryIdSpace3UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace3UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace4UserInfo() && profileInfo.proprietaryIdSpace4UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace4UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace5UserInfo() && profileInfo.proprietaryIdSpace5UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace5UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace6UserInfo() && profileInfo.proprietaryIdSpace6UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace6UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace7UserInfo() && profileInfo.proprietaryIdSpace7UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace7UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace8UserInfo() && profileInfo.proprietaryIdSpace8UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace8UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace9UserInfo() && profileInfo.proprietaryIdSpace9UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace9UserInfo.userId)
      }
      if (profileInfo.hasProprietaryIdSpace10UserInfo() && profileInfo.proprietaryIdSpace10UserInfo.hasUserId()) {
        return getFingerprint64Long(profileInfo.proprietaryIdSpace10UserInfo.userId)
      }
    } else if (labelerInput.hasEventId() && labelerInput.eventId.hasId()) {
      return getFingerprint64Long(labelerInput.eventId.id)
    }
    return (0..Long.MAX_VALUE).random()
  }
}
