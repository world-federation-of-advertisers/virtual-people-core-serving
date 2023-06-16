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

import com.google.protobuf.Descriptors.EnumValueDescriptor
import com.google.protobuf.Descriptors.FieldDescriptor
import com.google.protobuf.Descriptors.FieldDescriptor.Type
import org.wfanet.virtualpeople.common.ConditionalAssignment
import org.wfanet.virtualpeople.common.LabelerEvent
import org.wfanet.virtualpeople.common.fieldfilter.FieldFilter
import org.wfanet.virtualpeople.common.fieldfilter.utils.getFieldFromProto
import org.wfanet.virtualpeople.common.fieldfilter.utils.getValueFromProto
import org.wfanet.virtualpeople.common.fieldfilter.utils.setValueToProtoBuilder

internal class ConditionalAssignmentImpl
private constructor(private val condition: FieldFilter, private val assignments: List<Assignment>) :
  AttributesUpdaterInterface {

  /**
   * If [condition] is matched, for each entry in [assignments], assigns the value of source field
   * to target field. If [condition] is not matched, does nothing.
   */
  override fun update(event: LabelerEvent.Builder) {
    if (condition.matches(event)) {
      assignments.forEach { it.assign(event, it.source, it.target) }
    }
  }

  companion object {

    private inline fun <reified T> assign(
      event: LabelerEvent.Builder,
      source: List<FieldDescriptor>,
      target: List<FieldDescriptor>
    ) {
      val fieldValue = getValueFromProto<T>(event, source)
      if (!fieldValue.isSet) {
        return
      }
      setValueToProtoBuilder(event, target, fieldValue.value)
    }

    private fun getAssignmentFunction(
      type: Type
    ): (LabelerEvent.Builder, List<FieldDescriptor>, List<FieldDescriptor>) -> Unit {
      return when (type) {
        Type.INT32 -> { a, b, c -> assign<Int>(a, b, c) }
        Type.UINT32 -> { a, b, c -> assign<UInt>(a, b, c) }
        Type.INT64 -> { a, b, c -> assign<Long>(a, b, c) }
        Type.UINT64 -> { a, b, c -> assign<ULong>(a, b, c) }
        Type.BOOL -> { a, b, c -> assign<Boolean>(a, b, c) }
        Type.ENUM -> { a, b, c -> assign<EnumValueDescriptor>(a, b, c) }
        Type.STRING -> { a, b, c -> assign<String>(a, b, c) }
        else -> error("Unsupported field type for ConditionalAssignment: $type")
      }
    }

    /**
     * Always use [AttributesUpdaterInterface.build] to get an [AttributesUpdaterInterface] object.
     * Users should not call the factory method or the constructor of the derived classes directly.
     *
     * Returns error status when any of the following happens:
     * 1. [config].condition is not set.
     * 2. [config].assignments is empty.
     * 3. Fails to build a [FieldFilter] from [config].condition.
     * 4. In any entry of [config].assignments, sourceField or targetField is not set or does not
     *    refer to a valid field.
     * 5. In any entry of [config].assignments, sourceField and targetField refer to different type
     *    of fields. (like int32 vs int64)
     */
    internal fun build(config: ConditionalAssignment): ConditionalAssignmentImpl {
      if (!config.hasCondition()) {
        error("Condition is not set in ConditionalAssignment: $config")
      }
      if (config.assignmentsCount == 0) {
        error("No assignments in ConditionalAssignment: $config")
      }
      val condition = FieldFilter.create(LabelerEvent.getDescriptor(), config.condition)

      val assignments: List<Assignment> =
        config.assignmentsList.map { assignmentConfig ->
          if (!assignmentConfig.hasSourceField()) {
            error("All assignments must have source_field set in ConditionalAssignment: $config")
          }
          if (!assignmentConfig.hasTargetField()) {
            error("All assignments must have target_field set in ConditionalAssignment: $config")
          }
          val source: List<FieldDescriptor> =
            getFieldFromProto(LabelerEvent.getDescriptor(), assignmentConfig.sourceField)
          val target: List<FieldDescriptor> =
            getFieldFromProto(LabelerEvent.getDescriptor(), assignmentConfig.targetField)
          if (source.last().type != target.last().type) {
            error(
              "All assignments must have source_field and target_field being " +
                "the same type in ConditionalAssignment: $config "
            )
          }
          Assignment(source, target, getAssignmentFunction(source.last().type))
        }

      return ConditionalAssignmentImpl(condition, assignments)
    }
  }
}

private data class Assignment(
  val source: List<FieldDescriptor>,
  val target: List<FieldDescriptor>,
  val assign: (LabelerEvent.Builder, List<FieldDescriptor>, List<FieldDescriptor>) -> Unit
)
