/**
 *   Copyright 2011-2015 Quickstep Technologies LLC.
 *   Copyright 2015 Pivotal Software, Inc.
 *   Copyright 2016, Quickstep Research Group, Computer Sciences Department,
 *     University of Wisconsin—Madison.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 **/

#ifndef QUICKSTEP_EXPRESSIONS_AGGREGATION_AGGREGATION_HANDLE_AVG_HPP_
#define QUICKSTEP_EXPRESSIONS_AGGREGATION_AGGREGATION_HANDLE_AVG_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "catalog/CatalogTypedefs.hpp"
#include "expressions/aggregation/AggregationConcreteHandle.hpp"
#include "expressions/aggregation/AggregationHandle.hpp"
#include "storage/HashTableBase.hpp"
#include "storage/FastHashTable.hpp"
#include "threading/SpinMutex.hpp"
#include "types/Type.hpp"
#include "types/TypedValue.hpp"
#include "types/operations/binary_operations/BinaryOperation.hpp"
#include "utility/Macros.hpp"

#include "glog/logging.h"

namespace quickstep {

class ColumnVector;
class StorageManager;
class ValueAccessor;

/** \addtogroup Expressions
 *  @{
 */

/**
 * @brief Aggregation state for average.
 */
class AggregationStateAvg : public AggregationState {
 public:
  /**
   * @brief Copy constructor (ignores mutex).
   */
  AggregationStateAvg(const AggregationStateAvg &orig)
      : sum_(orig.sum_),
        count_(orig.count_),
        sum_offset(orig.sum_offset),
        count_offset(orig.count_offset),
        mutex_offset(orig.mutex_offset) {
  }

  /**
   * @brief Destructor.
   */
  ~AggregationStateAvg() override {}

  size_t getPayloadSize() const {
     size_t p1 = reinterpret_cast<size_t>(&sum_);
     size_t p2 = reinterpret_cast<size_t>(&mutex_);
     return (p2-p1);
  }

 private:
  friend class AggregationHandleAvg;

  AggregationStateAvg()
      : sum_(0), count_(0), sum_offset(0),
        count_offset(reinterpret_cast<uint8_t *>(&count_)-reinterpret_cast<uint8_t *>(&sum_)),
        mutex_offset(reinterpret_cast<uint8_t *>(&mutex_)-reinterpret_cast<uint8_t *>(&sum_)) {
  }

  // TODO(shoban): We might want to specialize sum_ and count_ to use atomics
  // for int types similar to in AggregationStateCount.
  TypedValue sum_;
  std::int64_t count_;
  SpinMutex mutex_;

  int sum_offset, count_offset, mutex_offset;
};

/**
 * @brief An aggregationhandle for avg.
 **/
class AggregationHandleAvg : public AggregationConcreteHandle {
 public:
  ~AggregationHandleAvg() override {
  }

  AggregationState* createInitialState() const override {
    return new AggregationStateAvg(blank_state_);
  }

  AggregationStateHashTableBase* createGroupByHashTable(
      const HashTableImplType hash_table_impl,
      const std::vector<const Type*> &group_by_types,
      const std::size_t estimated_num_groups,
      StorageManager *storage_manager) const override;

  /**
   * @brief Iterate method with average aggregation state.
   **/
  inline void iterateUnaryInl(AggregationStateAvg *state, const TypedValue &value) const {
    DCHECK(value.isPlausibleInstanceOf(argument_type_.getSignature()));
    if (value.isNull()) return;

    SpinMutexLock lock(state->mutex_);
    state->sum_ = fast_add_operator_->applyToTypedValues(state->sum_, value);
    ++state->count_;
  }

  inline void iterateUnaryInlFast(const TypedValue &value, uint8_t *byte_ptr) {
    DCHECK(value.isPlausibleInstanceOf(argument_type_.getSignature()));
    if (value.isNull()) return;
    TypedValue *sum_ptr = reinterpret_cast<TypedValue *>(byte_ptr + blank_state_.sum_offset);
    std::int64_t *count_ptr = reinterpret_cast<std::int64_t *>(byte_ptr + blank_state_.count_offset);
    *sum_ptr = fast_add_operator_->applyToTypedValues(*sum_ptr, value);
    ++(*count_ptr);
  }

  inline void iterateInlFast(const std::vector<TypedValue> &arguments, uint8_t *byte_ptr) override {
     iterateUnaryInlFast(arguments.front(), byte_ptr);
  }

  void initPayload(uint8_t *byte_ptr) override {
    TypedValue *sum_ptr = reinterpret_cast<TypedValue *>(byte_ptr + blank_state_.sum_offset);
    std::int64_t *count_ptr = reinterpret_cast<std::int64_t *>(byte_ptr + blank_state_.count_offset);
    *sum_ptr = blank_state_.sum_;
    *count_ptr = blank_state_.count_;
  }

  AggregationState* accumulateColumnVectors(
      const std::vector<std::unique_ptr<ColumnVector>> &column_vectors) const override;

#ifdef QUICKSTEP_ENABLE_VECTOR_COPY_ELISION_SELECTION
  AggregationState* accumulateValueAccessor(
      ValueAccessor *accessor,
      const std::vector<attribute_id> &accessor_id) const override;
#endif

  void aggregateValueAccessorIntoHashTable(
      ValueAccessor *accessor,
      const std::vector<attribute_id> &argument_ids,
      const std::vector<attribute_id> &group_by_key_ids,
      AggregationStateHashTableBase *hash_table) const override;

  void mergeStates(const AggregationState &source,
                   AggregationState *destination) const override;

  void mergeStatesFast(const uint8_t *source,
                   uint8_t *destination) const override;

  TypedValue finalize(const AggregationState &state) const override;

  inline TypedValue finalizeHashTableEntry(const AggregationState &state) const {
    const AggregationStateAvg &agg_state = static_cast<const AggregationStateAvg&>(state);
    // TODO(chasseur): Could improve performance further if we made a special
    // version of finalizeHashTable() that collects all the sums into one
    // ColumnVector and all the counts into another and then applies
    // '*divide_operator_' to them in bulk.
    return divide_operator_->applyToTypedValues(agg_state.sum_,
                                                TypedValue(static_cast<double>(agg_state.count_)));
  }

  inline TypedValue finalizeHashTableEntryFast(const uint8_t *byte_ptr) const {
//    const AggregationStateAvg &agg_state = static_cast<const AggregationStateAvg&>(state);
    // TODO(chasseur): Could improve performance further if we made a special
    // version of finalizeHashTable() that collects all the sums into one
    // ColumnVector and all the counts into another and then applies
    // '*divide_operator_' to them in bulk.

    uint8_t *value_ptr = const_cast<uint8_t*>(byte_ptr);
    TypedValue *sum_ptr = reinterpret_cast<TypedValue *>(value_ptr + blank_state_.sum_offset);
    std::int64_t *count_ptr = reinterpret_cast<std::int64_t *>(value_ptr + blank_state_.count_offset);
    return divide_operator_->applyToTypedValues(*sum_ptr,
                                                TypedValue(static_cast<double>(*count_ptr)));
  }

  ColumnVector* finalizeHashTable(
      const AggregationStateHashTableBase &hash_table,
      std::vector<std::vector<TypedValue>> *group_by_keys,
      int index) const override;

  /**
   * @brief Implementation of AggregationHandle::aggregateOnDistinctifyHashTableForSingle()
   *        for AVG aggregation.
   */
  AggregationState* aggregateOnDistinctifyHashTableForSingle(
      const AggregationStateHashTableBase &distinctify_hash_table) const override;

  /**
   * @brief Implementation of AggregationHandle::aggregateOnDistinctifyHashTableForGroupBy()
   *        for AVG aggregation.
   */
  void aggregateOnDistinctifyHashTableForGroupBy(
      const AggregationStateHashTableBase &distinctify_hash_table,
      AggregationStateHashTableBase *aggregation_hash_table) const override;

  void mergeGroupByHashTables(
      const AggregationStateHashTableBase &source_hash_table,
      AggregationStateHashTableBase *destination_hash_table) const override;

  size_t getPayloadSize() const override {
      return blank_state_.getPayloadSize();
  }

 private:
  friend class AggregateFunctionAvg;

  /**
   * @brief Constructor.
   *
   * @param type Type of the avg value.
   **/
  explicit AggregationHandleAvg(const Type &type);

  const Type &argument_type_;
  const Type *result_type_;
  AggregationStateAvg blank_state_;
  std::unique_ptr<UncheckedBinaryOperator> fast_add_operator_;
  std::unique_ptr<UncheckedBinaryOperator> merge_add_operator_;
  std::unique_ptr<UncheckedBinaryOperator> divide_operator_;

  DISALLOW_COPY_AND_ASSIGN(AggregationHandleAvg);
};

/** @} */

}  // namespace quickstep

#endif  // QUICKSTEP_EXPRESSIONS_AGGREGATION_AGGREGATION_HANDLE_AVG_HPP_
