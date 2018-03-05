#include "base_index_evaluator.hpp"

#include <algorithm>
#include <list>
#include <memory>
#include <string>

#include "logical_query_plan/abstract_lqp_node.hpp"
#include "logical_query_plan/predicate_node.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "operators/get_table.hpp"
#include "operators/table_scan.hpp"
#include "operators/validate.hpp"
#include "optimizer/column_statistics.hpp"
#include "optimizer/table_statistics.hpp"
#include "sql/gdfs_cache.hpp"
#include "sql/sql_query_cache.hpp"
#include "storage/index/base_index.hpp"
#include "storage/storage_manager.hpp"
#include "types.hpp"
#include "utils/logging.hpp"

namespace opossum {

BaseIndexEvaluator::BaseIndexEvaluator() {}

void BaseIndexEvaluator::evaluate(std::vector<std::shared_ptr<TuningChoice>>& choices) {
  // Allow concrete implementation to initialize
  _setup();

  // Scan query cache for indexable table column accesses
  _inspect_query_cache();

  // Aggregate column accesses to set of new columns to index
  _aggregate_access_records();

  // Fill index_evaluations vector
  _choices.clear();
  _add_existing_indexes();
  _add_new_indexes();

  // Evaluate
  for (auto& index_choice : _choices) {
    if (index_choice.exists) {
      index_choice.memory_cost = static_cast<float>(_existing_memory_cost(index_choice));
    } else {
      index_choice.type = _propose_index_type(index_choice);
      index_choice.memory_cost = static_cast<float>(_predict_memory_cost(index_choice));
    }
    index_choice.saved_work = _calculate_saved_work(index_choice);

    // Transfer results to choices vector
    choices.push_back(std::make_shared<IndexChoice>(index_choice));
  }
}

void BaseIndexEvaluator::_setup() {}

void BaseIndexEvaluator::_process_access_record(const BaseIndexEvaluator::AccessRecord&) {}

uintptr_t BaseIndexEvaluator::_existing_memory_cost(const IndexChoice& index_choice) const {
  const auto table = StorageManager::get().get_table(index_choice.column_ref.table_name);
  uintptr_t memory_cost = 0u;
  for (ChunkID chunk_id = ChunkID{0}; chunk_id < table->chunk_count(); ++chunk_id) {
    const auto chunk = table->get_chunk(chunk_id);
    const auto index = chunk->get_index(index_choice.type, index_choice.column_ref.column_ids);
    if (index) {
      memory_cost += index->memory_consumption();
    }
  }
  return memory_cost;
}

void BaseIndexEvaluator::_inspect_query_cache() {
  _access_records.clear();

  /*
   * ToDo(anybody): The cache interface could be improved by introducing values() method in
   * AbstractCache interface and implement in all subclasses
   *  const auto& query_plan_cache = SQLQueryOperator::get_query_plan_cache().cache();
   * -> so this implementation could be independent of the actual cache implementation
   */
  const auto& lqp_cache = SQLQueryCache<std::shared_ptr<AbstractLQPNode>>::get();
  // We cannot use dynamic_pointer_cast here because SQLQueryCache.cache() returns a reference, not a pointer
  auto gdfs_cache_ptr =
      dynamic_cast<const GDFSCache<std::string, std::shared_ptr<AbstractLQPNode>>*>(&(lqp_cache.cache()));
  if (!gdfs_cache_ptr) {
    LOG_WARN("BaseIndexEvaluator can only analyze GDFSCache instances! Evaluations may be useless...");
    return;
  }

  const auto& fibonacci_heap = gdfs_cache_ptr->queue();

  LOG_DEBUG("Query plan cache (size: " << fibonacci_heap.size() << "):");
  if (fibonacci_heap.size() == 0) {
    LOG_WARN("There are no logical query plans in the cache. Make sure that logical query plans get cached!");
  }

  auto cache_iterator = fibonacci_heap.ordered_begin();
  auto cache_end = fibonacci_heap.ordered_end();

  for (; cache_iterator != cache_end; ++cache_iterator) {
    const auto& entry = *cache_iterator;
    LOG_DEBUG("  -> Query '" << entry.key << "' frequency: " << entry.frequency << " priority: " << entry.priority);
    _inspect_lqp_operator(entry.value, entry.frequency);
  }
}

void BaseIndexEvaluator::_inspect_lqp_operator(const std::shared_ptr<const AbstractLQPNode>& op,
                                               size_t query_frequency) {
  std::list<const std::shared_ptr<const AbstractLQPNode>> queue;
  queue.push_back(op);
  while (!queue.empty()) {
    auto lqp_node = queue.front();
    queue.pop_front();

    if (auto left_child = lqp_node->left_input()) {
      queue.push_back(left_child);
    }
    if (auto right_child = lqp_node->right_input()) {
      queue.push_back(right_child);
    }

    switch (lqp_node->type()) {
      case LQPNodeType::Predicate: {
        /*
         * A PredicateNode represents a scan that could possibly be sped up by
         * creating an index. To do this, we need to find out where this column
         * came from, i.e. what column on what table is scanned
         */
        auto predicate_node = std::dynamic_pointer_cast<const PredicateNode>(lqp_node);
        DebugAssert(predicate_node, "LQP node is not actually a PredicateNode");
        auto lqp_ref = predicate_node->column_reference();
        // Follow the column reference to the node "producing" it (usually a StoredTableNode)
        if (lqp_ref.original_node()) {
          auto original_node = lqp_ref.original_node();
          auto original_column_id = original_node->find_output_column_id(lqp_ref);
          if (original_node->type() == LQPNodeType::StoredTable) {
            DebugAssert(original_column_id, "Could not find column ID for LQPColumnReference");
            auto stored_table = std::dynamic_pointer_cast<const StoredTableNode>(original_node);
            DebugAssert(stored_table, "referenced node is not actually a StoredTableNode");

            AccessRecord access_record(stored_table->table_name(), *original_column_id, query_frequency);
            access_record.condition = predicate_node->predicate_condition();
            access_record.compare_value = boost::get<AllTypeVariant>(predicate_node->value());
            _access_records.push_back(access_record);
          }
        }
      } break;
      case LQPNodeType::Join:
        // Probably interesting, room for future work
        break;
      default:
        // Not interesting
        break;
    }
  }
}

void BaseIndexEvaluator::_inspect_pqp_operator(const std::shared_ptr<const AbstractOperator>& op,
                                               size_t query_frequency) {
  std::list<const std::shared_ptr<const AbstractOperator>> queue;
  queue.push_back(op);
  while (!queue.empty()) {
    auto node = queue.front();
    queue.pop_front();
    if (const auto& table_scan = std::dynamic_pointer_cast<const TableScan>(node)) {
      /*
         * A TableScan represents a scan that could possibly be sped up by
         * creating an index. To do this, we need to find out where this column
         * came from, i.e. what column on what table is scanned.
         * Usually the underlying node is a GetTable node, holding this information.
         */
      DebugAssert(std::dynamic_pointer_cast<const Validate>(table_scan->input_left()) == nullptr,
                  "Validation nodes are not supported. Please run the pipeline without MVCC columns.");
      if (const auto& get_table = std::dynamic_pointer_cast<const GetTable>(table_scan->input_left())) {
        const auto& table_name = get_table->table_name();
        ColumnID column_id = table_scan->left_column_id();
        _access_records.emplace_back(table_name, column_id, query_frequency);
        _access_records.back().condition = table_scan->predicate_condition();
        _access_records.back().compare_value = boost::get<AllTypeVariant>(table_scan->right_parameter());
      }
    } else {
      if (node->input_left()) {
        queue.push_back(node->input_left());
      }
      if (node->input_right()) {
        queue.push_back(node->input_right());
      }
    }
  }
}

void BaseIndexEvaluator::_aggregate_access_records() {
  _new_indexes.clear();
  for (const auto& access_record : _access_records) {
    _new_indexes.insert(access_record.column_ref);
    _process_access_record(access_record);
  }
}

void BaseIndexEvaluator::_add_existing_indexes() {
  for (const auto& table_name : StorageManager::get().table_names()) {
    const auto& table = StorageManager::get().get_table(table_name);
    const auto& first_chunk = table->get_chunk(ChunkID{0});

    for (const auto& index_info : table->get_indexes()) {
      auto index_choice = IndexChoice{ColumnRef{table_name, index_info.column_ids}, true};
      index_choice.type = index_info.type;
      _choices.emplace_back(index_choice);
      _new_indexes.erase({table_name, index_info.column_ids});
    }
  }
}

void BaseIndexEvaluator::_add_new_indexes() {
  for (const auto& column_ref : _new_indexes) {
    _choices.emplace_back(column_ref, false);
  }
}

}  // namespace opossum
