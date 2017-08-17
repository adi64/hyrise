#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"

namespace opossum {

struct ColumnID;
class TableStatistics;

enum class ASTNodeType { Aggregate, Join, Predicate, Projection, Sort, StoredTable };

struct ColumnIdentifier {
  std::string table_name;
  std::string column_name;
};

/**
 * Abstract element in an Abstract Syntax Tree.
 * This tree is the base structure used by the optimizer to change the query plan.
 *
 *
 * Design decision:
 * We decided to have mutable Nodes for now.
 * By that we can apply rules without creating new nodes for every optimization rule.
 */
class AbstractASTNode : public std::enable_shared_from_this<AbstractASTNode> {
 public:
  explicit AbstractASTNode(ASTNodeType node_type);

  /**
   * The _parent is implicitly set in set_left_child/set_right_child.
   * For un-setting _parent use clear_parent().
   */
  std::shared_ptr<AbstractASTNode> parent() const;
  void clear_parent();

  const std::shared_ptr<AbstractASTNode> &left_child() const;
  void set_left_child(const std::shared_ptr<AbstractASTNode> &left);

  const std::shared_ptr<AbstractASTNode> &right_child() const;
  void set_right_child(const std::shared_ptr<AbstractASTNode> &right);

  ASTNodeType type() const;

  void set_statistics(const std::shared_ptr<TableStatistics> &statistics);
  const std::shared_ptr<TableStatistics> get_statistics();
  virtual const std::shared_ptr<TableStatistics> get_statistics_from(
      const std::shared_ptr<AbstractASTNode> &other_node) const;

  virtual const std::vector<std::string> output_column_names() const;
  virtual const std::vector<ColumnID> output_column_ids() const;
  bool has_output_column(const std::string &column_name) const;
  virtual const optional<ColumnID> find_column_id_for_column_identifier(ColumnIdentifier &column_identifier) const;
  virtual const bool manages_table(const std::string &table_name) const;

  void print(const uint32_t level = 0, std::ostream &out = std::cout) const;
  virtual std::string description() const = 0;

 protected:
  virtual const std::shared_ptr<TableStatistics> _gather_statistics() const;

  // Used to easily differentiate between node types without pointer casts.
  ASTNodeType _type;
  mutable std::vector<ColumnID> _output_column_ids;
  mutable std::vector<std::string> _output_column_names;

 private:
  std::weak_ptr<AbstractASTNode> _parent;
  std::shared_ptr<AbstractASTNode> _left_child;
  std::shared_ptr<AbstractASTNode> _right_child;

  std::shared_ptr<TableStatistics> _statistics;
};

}  // namespace opossum
