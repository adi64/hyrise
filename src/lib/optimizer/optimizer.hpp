#pragma once

#include <memory>
#include <vector>

#include "optimizer/abstract_syntax_tree/abstract_ast_node.hpp"

namespace opossum {

class AbstractRule;
class ASTRootNode;

/**
 * Applies (currently: all) optimization rules to an AST.
 */
class Optimizer final {
 public:
  static const Optimizer& get();

  Optimizer();

  std::shared_ptr<AbstractASTNode> optimize(const std::shared_ptr<AbstractASTNode>& input) const;

 private:
  std::vector<std::shared_ptr<AbstractRule>> _rules;

  // Rather arbitrary right now, atm all rules should be done after one iteration
  uint32_t _max_num_iterations = 10;
};

}  // namespace opossum
