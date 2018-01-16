#include "system_statistics.hpp"

#include "sql/sql_query_cache.hpp"
#include "sql/sql_query_operator.hpp"

namespace opossum {

SystemStatistics::SystemStatistics(const SQLQueryCache<SQLQueryPlan>& cache) : _recent_queries{}, _cache(cache) {}

const std::vector<SystemStatistics::SQLQueryCacheEntry>& SystemStatistics::recent_queries() const {
  // TODO(group01) lazily initialize this and update only if there were changes
  _recent_queries.clear();

  // TODO(group01) introduce values() method in AbstractCache interface and implement in all subclasses
  // const auto& query_plan_cache = SQLQueryOperator::get_query_plan_cache().cache();
  const auto& query_plan_cache = _cache;
  // TODO(group01) implement for cache implementations other than GDFS cache
  auto gdfs_cache_ptr = dynamic_cast<const GDFSCache<std::string, SQLQueryPlan>*>(&query_plan_cache.cache());
  Assert(gdfs_cache_ptr, "Expected GDFS Cache");

  const boost::heap::fibonacci_heap<GDFSCache<std::string, SQLQueryPlan>::GDFSCacheEntry>& fibonacci_heap =
      gdfs_cache_ptr->queue();
  std::cout << "Query plan cache size: " << fibonacci_heap.size() << "\n";
  auto cache_iterator = fibonacci_heap.ordered_begin();
  auto cache_end = fibonacci_heap.ordered_end();

  for (; cache_iterator != cache_end; ++cache_iterator) {
    const GDFSCache<std::string, SQLQueryPlan>::GDFSCacheEntry& query_plan = *cache_iterator;
    std::cout << "Query '" << query_plan.key << "' frequency: " << query_plan.frequency << " priority: " << query_plan.priority << "\n";
    _recent_queries.push_back({query_plan.key, query_plan.value, query_plan.frequency});
  }

  return _recent_queries;
}

}  // namespace opossum
