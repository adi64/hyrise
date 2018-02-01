#include "base_index_evaluator.hpp"

#include <algorithm>
#include <iostream>
#include <list>

#include "operators/get_table.hpp"
#include "operators/table_scan.hpp"
#include "operators/validate.hpp"
#include "optimizer/column_statistics.hpp"
#include "optimizer/table_statistics.hpp"
#include "storage/index/base_index.hpp"
#include "storage/storage_manager.hpp"
#include "types.hpp"
#include "utils/logging.hpp"

namespace opossum {

BaseIndexEvaluator::BaseIndexEvaluator() {}

std::vector<IndexEvaluation> BaseIndexEvaluator::evaluate_indices(const SystemStatistics& statistics) {
  // Scan query cache for indexable table column accesses
  _inspect_query_cache(statistics.cache());

  // Aggregate column accesses to set of new columns to index
  _aggregate_access_records();

  // Fill index_evaluations vector
  _evaluations.clear();
  _add_existing_indices();
  _add_new_indices();

  // Evaluate
  for (auto& index_evaluation : _evaluations) {
    if (index_evaluation.exists) {
      index_evaluation.memory_cost = _existing_memory_cost(index_evaluation);
    } else {
      index_evaluation.type = _propose_index_type(index_evaluation);
      index_evaluation.memory_cost = _predict_memory_cost(index_evaluation);
    }
    index_evaluation.desirablility = _calculate_desirability(index_evaluation);
  }

  return std::vector<IndexEvaluation>(_evaluations);
}

void BaseIndexEvaluator::_setup() {}

void BaseIndexEvaluator::_process_access_record(const BaseIndexEvaluator::AccessRecord& /*record*/) {}

float BaseIndexEvaluator::_existing_memory_cost(const IndexEvaluation& index_evaluation) const {
  auto table = StorageManager::get().get_table(index_evaluation.column.table_name);
  float memory_cost = 0.0f;
  for (ChunkID chunk_id = ChunkID{0}; chunk_id < table->chunk_count(); ++chunk_id) {
    auto chunk = table->get_chunk(chunk_id);
    auto index = chunk->get_index(index_evaluation.type, std::vector<ColumnID>{index_evaluation.column.column_id});
    if (index) {
      memory_cost += index->memory_consumption();
    }
  }
  return memory_cost;
}

void BaseIndexEvaluator::_inspect_query_cache(const SQLQueryCache<std::shared_ptr<SQLQueryPlan>>& cache) {
  _access_records.clear();

  // ToDo(group01) introduce values() method in AbstractCache interface and implement in all subclasses
  //   const auto& query_plan_cache = SQLQueryOperator::get_query_plan_cache().cache();
  // ToDo(group01) implement for cache implementations other than GDFS cache
  auto gdfs_cache_ptr = dynamic_cast<const GDFSCache<std::string, std::shared_ptr<SQLQueryPlan>>*>(&cache.cache());
  Assert(gdfs_cache_ptr, "Expected GDFS Cache");

  const boost::heap::fibonacci_heap<GDFSCache<std::string, std::shared_ptr<SQLQueryPlan>>::GDFSCacheEntry>&
      fibonacci_heap = gdfs_cache_ptr->queue();

  LOG_DEBUG("Query plan cache (size: " << fibonacci_heap.size() << "):");
  auto cache_iterator = fibonacci_heap.ordered_begin();
  auto cache_end = fibonacci_heap.ordered_end();

  for (; cache_iterator != cache_end; ++cache_iterator) {
    const GDFSCache<std::string, std::shared_ptr<SQLQueryPlan>>::GDFSCacheEntry& entry = *cache_iterator;
    LOG_DEBUG("  -> Query '" << entry.key << "' frequency: " << entry.frequency << " priority: " << entry.priority);
    for (const auto& operator_tree : entry.value->tree_roots()) {
      _inspect_operator(operator_tree, entry.frequency);
    }
  }
}

void BaseIndexEvaluator::_inspect_operator(const std::shared_ptr<const AbstractOperator>& op, size_t query_frequency) {
  std::list<const std::shared_ptr<const AbstractOperator>> queue;
  queue.push_back(op);
  while (!queue.empty()) {
    auto node = queue.front();
    queue.pop_front();
    if (const auto& table_scan = std::dynamic_pointer_cast<const TableScan>(node)) {
      if (const auto& validate = std::dynamic_pointer_cast<const Validate>(table_scan->input_left())) {
        if (const auto& get_table = std::dynamic_pointer_cast<const GetTable>(validate->input_left())) {
          const auto& table_name = get_table->table_name();
          ColumnID column_id = table_scan->left_column_id();
          _access_records.emplace_back(table_name, column_id, query_frequency);
          _access_records.back().condition = table_scan->predicate_condition();
          _access_records.back().compare_value = boost::get<AllTypeVariant>(table_scan->right_parameter());
        }
      }
    } else {
      if (op->input_left()) {
        queue.push_back(op->input_left());
      }
      if (op->input_right()) {
        queue.push_back(op->input_right());
      }
    }
  }
}

void BaseIndexEvaluator::_aggregate_access_records() {
  _new_indices.clear();
  for (const auto& access_record : _access_records) {
    _new_indices.insert(access_record.column_ref);
    _process_access_record(access_record);
  }
}

void BaseIndexEvaluator::_add_existing_indices() {
  for (const auto& table_name : StorageManager::get().table_names()) {
    const auto& table = StorageManager::get().get_table(table_name);
    const auto& first_chunk = table->get_chunk(ChunkID{0});

    for (const auto& column_name : table->column_names()) {
      const auto& column_id = table->column_id_by_name(column_name);
      auto column_ids = std::vector<ColumnID>();
      column_ids.emplace_back(column_id);
      auto indices = first_chunk->get_indices(column_ids);
      for (const auto& index : indices) {
        _evaluations.emplace_back(table_name, column_id, true);
        _evaluations.back().type = index->type();
        _new_indices.erase({table_name, column_id});
      }
      if (indices.size() > 1) {
        LOG_DEBUG("Found " << indices.size() << " indices on " << table_name << "." << column_name);
      } else if (indices.size() > 0) {
        LOG_DEBUG("Found index on " << table_name << "." << column_name);
      }
    }
  }
}

void BaseIndexEvaluator::_add_new_indices() {
  for (const auto& index_spec : _new_indices) {
    _evaluations.emplace_back(index_spec.table_name, index_spec.column_id, false);
    _evaluations.back().type = _propose_index_type(_evaluations.back());
  }
}

}  // namespace opossum