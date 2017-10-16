#pragma once

#include "optimizer/strategy/strategy_base_test.hpp"
#include "optimizer/column_statistics.hpp"
#include "optimizer/strategy/join_ordering/join_graph.hpp"
#include "optimizer/table_statistics.hpp"
#include "optimizer/abstract_syntax_tree/mock_table_node.hpp"

namespace opossum {

/**
 * Mock a table with one column containing all integer values in a specified [min, max] range
 */
class JoinOrderingTableStatistics: public TableStatistics {
 public:
  JoinOrderingTableStatistics(int32_t min, int32_t max, float row_count):
    TableStatistics(row_count, 1) {
    Assert(min <= max, "min value should be smaller than max value");
    _column_statistics[0] = std::make_shared<ColumnStatistics<int32_t>>(ColumnID{0}, row_count, min, max,
                                                                                0.0f);
  }
};

class JoinReorderingBaseTest: public StrategyBaseTest {
 public:
  JoinReorderingBaseTest() {
    const auto make_mock_table = [] (int32_t min, int32_t max, float row_count) {
      return std::make_shared<MockTableNode>(
        std::make_shared<JoinOrderingTableStatistics>(min, max, row_count)
      );
    };

    const auto make_complete_join_graph = [] (std::vector<std::shared_ptr<AbstractASTNode>> vertices) {
      JoinGraph::Edges edges;

      for (size_t vertex_idx_a = 0; vertex_idx_a < vertices.size(); ++vertex_idx_a) {
        for (size_t vertex_idx_b = 0; vertex_idx_b < vertices.size(); ++vertex_idx_b) {
          if (vertex_idx_a == vertex_idx_b) {
            continue;
          }

          JoinEdge edge;
          edge.vertex_indices = {vertex_idx_a, vertex_idx_b};

          JoinPredicate predicate;
          predicate.mode = JoinMode::Inner;
          predicate.column_ids = {ColumnID{0}, ColumnID{0}};
          predicate.scan_type = ScanType::OpEquals;

          edge.predicate = predicate;

          edges.emplace_back(edge);
        }
      }

      return std::make_shared<JoinGraph>(std::move(vertices), std::move(edges));
    };
    const auto make_chain_join_graph = [] (std::vector<std::shared_ptr<AbstractASTNode>> vertices) {
      JoinGraph::Edges edges;

      for (size_t vertex_idx_a = 1; vertex_idx_a < vertices.size(); ++vertex_idx_a) {
        const auto vertex_idx_b = vertex_idx_a - 1;

        JoinEdge edge;
        edge.vertex_indices = {vertex_idx_a, vertex_idx_b};

        JoinPredicate predicate;
        predicate.mode = JoinMode::Inner;
        predicate.column_ids = {ColumnID{0}, ColumnID{0}};
        predicate.scan_type = ScanType::OpEquals;

        edge.predicate = predicate;

        edges.emplace_back(edge);
      }

      return std::make_shared<JoinGraph>(std::move(vertices), std::move(edges));
    };

    _table_node_a = make_mock_table(10, 80, 70);
    _table_node_b = make_mock_table(10, 60, 60);
    _table_node_c = make_mock_table(50, 100, 15);
    _table_node_d = make_mock_table(53, 57, 10);
    _table_node_e = make_mock_table(40, 90, 600);

    _join_graph_cde_chain = make_chain_join_graph({_table_node_c, _table_node_d, _table_node_e});
    _join_graph_cde_complete = make_complete_join_graph({_table_node_c, _table_node_d, _table_node_e});
  }

 protected:
  std::shared_ptr<MockTableNode> _table_node_a;
  std::shared_ptr<MockTableNode> _table_node_b;
  std::shared_ptr<MockTableNode> _table_node_c;
  std::shared_ptr<MockTableNode> _table_node_d;
  std::shared_ptr<MockTableNode> _table_node_e;
  std::shared_ptr<JoinGraph> _join_graph_cde_chain;
  std::shared_ptr<JoinGraph> _join_graph_cde_complete;
};

}