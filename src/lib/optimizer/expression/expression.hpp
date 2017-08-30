#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "all_type_variant.hpp"
#include "common.hpp"
#include "types.hpp"

namespace opossum {

class AbstractASTNode;

/**
 * The basic idea of Expressions is to have a unified representation of any SQL Expressions within Hyrise
 * and especially its optimizer.
 *
 * Expressions are structured as a binary tree
 * e.g. 'columnA = 5' would be represented as a root expression with the type ExpressionType::Equals and
 * two child nodes of types ExpressionType::ColumnIdentifier and ExpressionType::Literal.
 *
 * For now we decided to have a single Expression without further specializations. This goes hand in hand with the
 * approach used in hsql::Expr.
 */
class Expression : public std::enable_shared_from_this<Expression> {
 public:
  /*
   * This constructor is meant for internal use only and therefor should be private.
   * However, in C++ one is not able to call std::make_shared with a private constructor.
   * The naive approach of befriending std::make_shared does not work here, as the implementation of std::make_shared is
   * compiler-specific and usually relies on internal impl-classes.
   * (e.g.:
   * https://stackoverflow.com/questions/3541632/using-make-shared-with-a-protected-constructor-abstract-interface)
   * We refrained from using the suggested pass-key-idiom as it only increases complexity but does not help removing a
   * public constructor.
   *
   * In the end we debated between creating the shared_ptr explicitly in the factory methods
   * and making the constructor public. For now we decided to follow the latter.
   *
   * We highly suggest using one of the create_*-methods over using this constructor.
   */
  explicit Expression(ExpressionType type);

  /*
   * Factory Methods to create Expressions of specific type
   */
  static std::shared_ptr<Expression> create_column_identifier(const ColumnID column_id,
                                                              const optional<std::string>& alias = nullopt);

  static std::vector<std::shared_ptr<Expression>> create_column_identifiers(
      const std::vector<ColumnID>& column_ids, const optional<std::vector<std::string>>& aliases = nullopt);

  // A literal can have an alias in order to allow queries like `SELECT 1 as one FROM t`.
  static std::shared_ptr<Expression> create_literal(const AllTypeVariant& value,
                                                    const optional<std::string>& alias = nullopt);

  static std::shared_ptr<Expression> create_placeholder(const AllTypeVariant &value);

  static std::shared_ptr<Expression> create_function(
    const std::string &function_name, const std::vector<std::shared_ptr<Expression>> &expression_list,
    const optional <std::string> &alias = nullopt);

  static std::shared_ptr<Expression> create_binary_operator(ExpressionType type,
                                                            const std::shared_ptr<Expression>& left,
                                                            const std::shared_ptr<Expression>& right,
                                                            const optional<std::string>& alias = nullopt);

  static std::shared_ptr<Expression> create_select_star(const std::string& table_name);

  // @{
  /**
   * Helper methods for Expression Trees, set_left_child() and set_right_child() will set parent
   */
  const std::weak_ptr<Expression> parent() const;
  void clear_parent();

  const std::shared_ptr<Expression> left_child() const;
  void set_left_child(const std::shared_ptr<Expression>& left);

  const std::shared_ptr<Expression> right_child() const;
  void set_right_child(const std::shared_ptr<Expression>& right);
  // @}

  const ExpressionType type() const;

  /*
   * Methods for debug printing
   */
  void print(const uint32_t level = 0, std::ostream& out = std::cout) const;

  const std::string description() const;

  // Is +, -, * (arithmetic usage, not SELECT * FROM), /, %, ^
  bool is_arithmetic_operator() const;

  // Returns true if the expression is a literal or column reference.
  bool is_operand() const;

  // Returns true if the expression requires two children.
  bool is_binary_operator() const;

  const ColumnID column_id() const;

  void set_column_id(const ColumnID column_id);

  const std::string& name() const;

  const optional<std::string>& alias() const;

  void set_alias(const std::string& alias);

  const AllTypeVariant value() const;

  const std::vector<std::shared_ptr<Expression>>& expression_list() const;

  void set_expression_list(const std::vector<std::shared_ptr<Expression>>& expression_list);

  // Expression as string
  std::string to_string(const std::shared_ptr<AbstractASTNode>& input_node = {}) const;

  bool operator==(const Expression& rhs) const;

 private:
  // the type of the expression
  const ExpressionType _type;
  // the value of an expression, e.g. of a Literal
  optional<AllTypeVariant> _value;

  /*
   * A list of Expressions used in FunctionIdentifiers and CASE Expressions.
   * Not sure if this is the perfect way to go forward, but this is how hsql::Expr handles this.
   *
   * In case there are at most two expressions in this list, one could replace this list with an additional layer in the
   * Expression hierarchy.
   * E.g. for CASE one could argue that the THEN case becomes the left child, whereas ELSE becomes the right child.
   */
  std::vector<std::shared_ptr<Expression>> _expression_list;

  // a name, which could be a function name
  optional<std::string> _name;

  // a column that might be referenced
  optional<ColumnID> _column_id;

  // an alias, used for ColumnReferences, Selects, FunctionIdentifiers
  optional<std::string> _alias;

  // @{
  // Members for the tree strucutre
  std::weak_ptr<Expression> _parent;
  std::shared_ptr<Expression> _left_child;
  std::shared_ptr<Expression> _right_child;
  // @}
};

}  // namespace opossum