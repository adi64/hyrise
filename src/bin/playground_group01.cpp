#include <chrono>
#include <iostream>

#include "concurrency/transaction_manager.hpp"
#include "operators/import_binary.hpp"
#include "logical_query_plan/abstract_lqp_node.hpp"
#include "logical_query_plan/lqp_column_reference.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "logical_query_plan/predicate_node.hpp"
#include "sql/sql_pipeline.hpp"
#include "sql/sql_query_cache.hpp"
#include "sql/sql_query_operator.hpp"
#include "storage/storage_manager.hpp"
#include "tpcc/tpcc_table_generator.hpp"
#include "tuning/index_tuner.hpp"
#include "tuning/system_statistics.hpp"
#include "utils/assert.hpp"

using std::chrono::high_resolution_clock;

// Test set of queries - for development.
// ToDo(group01): as soon as caching is integrated into the SQLPipeline, we should run a bigger and more standardized
//                workload, e.g. the TPC-C benchmark
// Idea behind the current queries: have three indexable columns, but one only used once, one twice, and one thrice.

const std::vector<std::string> test_queries{"SELECT BALANCE FROM CUSTOMER WHERE NAME = 'Danni Cohdwell'",
                                            "SELECT NAME FROM CUSTOMER WHERE LEVEL = 5",
                                            "SELECT BALANCE FROM CUSTOMER WHERE NAME = 'Danni Cohdwell'",
                                            "SELECT NAME FROM CUSTOMER WHERE LEVEL = 4",
                                            "SELECT BALANCE FROM CUSTOMER WHERE NAME = 'Danni Cohdwell'",
                                            "SELECT NAME FROM CUSTOMER WHERE LEVEL = 3",
                                            "SELECT INTEREST FROM CUSTOMER WHERE NAME  = 'Rosemary Picardi'",
                                            "SELECT BALANCE FROM CUSTOMER WHERE NAME = 'Danni Cohdwell'"};

// Forward declarations
std::shared_ptr<opossum::SQLPipeline> _create_and_cache_pipeline(
    const std::string& query, opossum::SQLQueryCache<std::shared_ptr<opossum::SQLQueryPlan>>& cache);
int _execute_query(const std::string& query, unsigned int execution_count,
                   opossum::SQLQueryCache<std::shared_ptr<opossum::SQLQueryPlan>>& cache);

int main() {
  opossum::SQLQueryCache<std::shared_ptr<opossum::SQLQueryPlan>> cache(1024);
  auto statistics = std::make_shared<opossum::SystemStatistics>(cache);
  opossum::IndexTuner tuner(statistics);

  std::cout << "Loading binary table...\n";
  auto importer = std::make_shared<opossum::ImportBinary>("group01_CUSTOMER.bin", "CUSTOMER");
  importer->execute();
  std::cout << "Table loaded.\n";





  auto pipeline = std::make_shared<opossum::SQLPipeline>("select NAME, BALANCE from CUSTOMER as c1 inner join CUSTOMER as c2 on c1.LEVEL = c2.ID where LEVEL = 5 LIMIT 20");
  auto lqp = pipeline->get_optimized_logical_plans();
  auto nodes_todo = lqp;
  while (!nodes_todo.empty()) {
      auto lqp_node = nodes_todo.back();
      nodes_todo.pop_back();

      if (auto left_child = lqp_node->left_child()) {
          nodes_todo.push_back(left_child);
      }
      if (auto right_child = lqp_node->right_child()) {
          nodes_todo.push_back(right_child);
      }

      std::cout << "LQP node: " << lqp_node->description() << "\n";

      switch (lqp_node->type()) {
      case opossum::LQPNodeType::Predicate :
      {
          auto predicate_node = std::dynamic_pointer_cast<const opossum::PredicateNode>(lqp_node);
          Assert(predicate_node, "LQP node is not actually a PredicateNode");
          auto lqp_ref = predicate_node->column_reference();
          std::cout << "column reference: " << lqp_ref.description() << "\n";
          std::cout << "Column " << lqp_ref.original_column_id() << " of node " << lqp_ref.original_node() << "\n";
          if (lqp_ref.original_node()) {
              auto original_node = lqp_ref.original_node();
              std::cout << "original node: " << original_node->description() << "\n";
              auto original_columnID = original_node->find_output_column_id(lqp_ref);
              if (original_columnID) {
                std::cout << "column ID there: " << *original_columnID << "\n";
              }
              if (original_node->type() == opossum::LQPNodeType::StoredTable) {
                  std::cout << "original node is StoredTable node\n";
                  auto storedTable = std::dynamic_pointer_cast<const opossum::StoredTableNode>(original_node);
                  opossum::Assert(storedTable, "failed dynamic cast");
                  std::cout << "original table name: " << storedTable->table_name() << "\n";
                  std::cout << "original column name: " << opossum::StorageManager::get().get_table(storedTable->table_name())->column_name(*original_columnID) << "\n";
              }
          }
      }
          break;
      case opossum::LQPNodeType::Join :
          // Probably interesting
          break;
      default:
          // Not interesting
          break;
      }
  }

  return 1;





  constexpr unsigned int execution_count = 5;

  std::vector<int> first_execution_times(test_queries.size());
  std::vector<int> second_execution_times(test_queries.size());

  std::cout << "Executing queries a first time to fill up the cache...\n";
  // Fire SQL query and cache it
  for (auto query_index = 0u; query_index < test_queries.size(); ++query_index) {
    first_execution_times[query_index] = _execute_query(test_queries[query_index], execution_count, cache);
  }

  // Let the tuner optimize tables based on the values of the cache
  std::cout << "Execute IndexTuner...\n";
  tuner.execute();

  std::cout << "Executing queries a second time (with optimized indices)...\n";
  std::cout << "Execution times (microseconds):\n";

  // Execute the same queries a second time and measure the speedup
  for (auto query_index = 0u; query_index < test_queries.size(); ++query_index) {
    second_execution_times[query_index] = _execute_query(test_queries[query_index], execution_count, cache);

    float percentage = (static_cast<float>(second_execution_times[query_index]) /
                        static_cast<float>(first_execution_times[query_index])) *
                       100.0;
    std::cout << "Query: " << test_queries[query_index] << " reduced to: " << percentage << "%\n";
    std::cout << "  before/after: " << first_execution_times[query_index] << " / "
              << second_execution_times[query_index] << "\n";
  }

  std::cout << "Execute IndexTuner AGAIN (sanity check)...\n";
  tuner.execute();

  return 0;
}

// Creates a Pipeline based on the supplied query and puts its query plan in the supplied cache
std::shared_ptr<opossum::SQLPipeline> _create_and_cache_pipeline(
    const std::string& query, opossum::SQLQueryCache<std::shared_ptr<opossum::SQLQueryPlan>>& cache) {
  auto pipeline = std::make_shared<opossum::SQLPipeline>(query);

  auto query_plans = pipeline->get_query_plans();

  // ToDo(group01): What is the semantics of multiple entries per query? Handle cases accordingly.
  opossum::Assert(query_plans.size() == 1, "Expected only one query plan per pipeline");
  cache.set(query, query_plans[0]);
  return pipeline;
}

// Executes a query repeatedly and measures the execution time
int _execute_query(const std::string& query, unsigned int execution_count,
                   opossum::SQLQueryCache<std::shared_ptr<opossum::SQLQueryPlan>>& cache) {
  int accumulated_duration = 0;

  // Execute queries multiple times to get more stable timing results
  for (auto counter = 0u; counter < execution_count; counter++) {
    auto pipeline = _create_and_cache_pipeline(query, cache);
    pipeline->get_result_table();
    accumulated_duration += pipeline->execution_time_microseconds().count();
  }

  return accumulated_duration / execution_count;
}
