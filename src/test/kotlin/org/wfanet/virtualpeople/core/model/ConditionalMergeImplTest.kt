// Copyright 2022 The Cross-Media Measurement Authors
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

import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import org.wfanet.virtualpeople.common.BranchNodeKt.attributesUpdater
import org.wfanet.virtualpeople.common.ConditionalMergeKt.conditionalMergeNode
import org.wfanet.virtualpeople.common.FieldFilterProto.Op
import org.wfanet.virtualpeople.common.conditionalMerge
import org.wfanet.virtualpeople.common.fieldFilterProto
import org.wfanet.virtualpeople.common.labelerEvent
import org.wfanet.virtualpeople.common.labelerInput

@RunWith(JUnit4::class)
class ConditionalMergeImplTest {

  @Test
  fun `no node should throw`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge { passThroughNonMatches = false }
    }
    val error = assertFailsWith<IllegalStateException> { AttributesUpdaterInterface.build(config) }
    assertTrue(error.message!!.contains("No nodes in ConditionalMerge"))
  }

  @Test
  fun `no condition should throw`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode { update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_1" } }
        )
        passThroughNonMatches = false
      }
    }
    val error = assertFailsWith<IllegalStateException> { AttributesUpdaterInterface.build(config) }
    assertTrue(error.message!!.contains("No condition in the node"))
  }

  @Test
  fun `invalid condition should throw`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto { op = Op.EQUAL }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_1" }
          }
        )
        passThroughNonMatches = false
      }
    }
    val error = assertFailsWith<IllegalStateException> { AttributesUpdaterInterface.build(config) }
    assertTrue(error.message!!.contains("Name must be set."))
  }

  @Test
  fun `no update should throw`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_1"
            }
          }
        )
        passThroughNonMatches = false
      }
    }
    val error = assertFailsWith<IllegalStateException> { AttributesUpdaterInterface.build(config) }
    assertTrue(error.message!!.contains("No update in the node"))
  }

  @Test
  fun `update event successfully`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_1"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_1" }
          }
        )
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_2"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_2" }
          }
        )
        passThroughNonMatches = false
      }
    }
    val updater = AttributesUpdaterInterface.build(config)

    /** Matches the 1st node, update the event. */
    val eventBuilder1 = labelerEvent { personCountryCode = "COUNTRY_1" }.toBuilder()
    updater.update(eventBuilder1)
    assertEquals("UPDATED_COUNTRY_1", eventBuilder1.personCountryCode)

    /** Matches the 2st node, update the event. */
    val eventBuilder2 = labelerEvent { personCountryCode = "COUNTRY_2" }.toBuilder()
    updater.update(eventBuilder2)
    assertEquals("UPDATED_COUNTRY_2", eventBuilder2.personCountryCode)
  }

  @Test
  fun `nested placement values map to categories with pass through`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              name = "labeler_input.placement"
              op = Op.IN
              value = "PLACEMENT_UNSPECIFIED,FACEBOOK_DESKTOP_NEWS_FEED,FACEBOOK_INSTREAM"
            }
            update = labelerEvent { labelerInput = labelerInput { placement = "facebook" } }
          }
        )
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              name = "labeler_input.placement"
              op = Op.IN
              value = "INSTAGRAM_EXPLORE,INSTAGRAM_FEED,INSTAGRAM_REELS,INSTAGRAM_STORIES"
            }
            update = labelerEvent { labelerInput = labelerInput { placement = "instagram" } }
          }
        )
        passThroughNonMatches = true
      }
    }
    val updater = AttributesUpdaterInterface.build(config)

    val facebookEvent =
      labelerEvent {
          labelerInput = labelerInput {
            timestampUsec = 123L
            placement = "FACEBOOK_INSTREAM"
          }
        }
        .toBuilder()
    updater.update(facebookEvent)
    assertEquals("facebook", facebookEvent.labelerInput.placement)
    assertEquals(123L, facebookEvent.labelerInput.timestampUsec)

    val instagramEvent =
      labelerEvent {
          labelerInput = labelerInput {
            timestampUsec = 456L
            placement = "INSTAGRAM_REELS"
          }
        }
        .toBuilder()
    updater.update(instagramEvent)
    assertEquals("instagram", instagramEvent.labelerInput.placement)
    assertEquals(456L, instagramEvent.labelerInput.timestampUsec)

    val unknownEvent =
      labelerEvent {
          labelerInput = labelerInput {
            timestampUsec = 789L
            placement = "UNKNOWN_PLACEMENT"
          }
        }
        .toBuilder()
    updater.update(unknownEvent)
    assertEquals("UNKNOWN_PLACEMENT", unknownEvent.labelerInput.placement)
    assertEquals(789L, unknownEvent.labelerInput.timestampUsec)
  }

  @Test
  fun `nested placement values map to categories without pass through`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              name = "labeler_input.placement"
              op = Op.IN
              value = "FACEBOOK_DESKTOP_NEWS_FEED,FACEBOOK_INSTREAM"
            }
            update = labelerEvent { labelerInput = labelerInput { placement = "facebook" } }
          }
        )
        passThroughNonMatches = false
      }
    }
    val updater = AttributesUpdaterInterface.build(config)

    val matchingEvent =
      labelerEvent { labelerInput = labelerInput { placement = "FACEBOOK_INSTREAM" } }.toBuilder()
    updater.update(matchingEvent)
    assertEquals("facebook", matchingEvent.labelerInput.placement)

    val unknownEvent =
      labelerEvent { labelerInput = labelerInput { placement = "UNKNOWN_PLACEMENT" } }.toBuilder()
    val exception = assertFailsWith<IllegalStateException> { updater.update(unknownEvent) }
    assertTrue(exception.message!!.contains("No node matching"))
    assertEquals("UNKNOWN_PLACEMENT", unknownEvent.labelerInput.placement)
  }

  @Test
  fun `no matching not pass should throw`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_1"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_1" }
          }
        )
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_2"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_2" }
          }
        )
        passThroughNonMatches = false
      }
    }
    val updater = AttributesUpdaterInterface.build(config)

    /** No matching. Returns error status as pass_through_non_matches is false. */
    val eventBuilder = labelerEvent { personCountryCode = "COUNTRY_3" }.toBuilder()
    val exception = assertFailsWith<IllegalStateException> { updater.update(eventBuilder) }
    assertTrue(exception.message!!.contains("No node matching"))
    assertEquals("COUNTRY_3", eventBuilder.personCountryCode)
  }

  @Test
  fun `no matching not pass by default should throw`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_1"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_1" }
          }
        )
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_2"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_2" }
          }
        )
      }
    }
    val updater = AttributesUpdaterInterface.build(config)

    /** No matching. Returns error status as pass_through_non_matches is false by default. */
    val eventBuilder = labelerEvent { personCountryCode = "COUNTRY_3" }.toBuilder()
    val exception = assertFailsWith<IllegalStateException> { updater.update(eventBuilder) }
    assertTrue(exception.message!!.contains("No node matching"))
    assertEquals("COUNTRY_3", eventBuilder.personCountryCode)
  }

  @Test
  fun `no matching pass should do nothing`() {
    val config = attributesUpdater {
      conditionalMerge = conditionalMerge {
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_1"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_1" }
          }
        )
        nodes.add(
          conditionalMergeNode {
            condition = fieldFilterProto {
              op = Op.EQUAL
              name = "person_country_code"
              value = "COUNTRY_2"
            }
            update = labelerEvent { personCountryCode = "UPDATED_COUNTRY_2" }
          }
        )
        passThroughNonMatches = true
      }
    }
    val updater = AttributesUpdaterInterface.build(config)

    /** No matching. Do nothing as pass_through_non_matches is true. */
    val eventBuilder = labelerEvent { personCountryCode = "COUNTRY_3" }.toBuilder()
    updater.update(eventBuilder)
    assertEquals("COUNTRY_3", eventBuilder.personCountryCode)
  }
}
