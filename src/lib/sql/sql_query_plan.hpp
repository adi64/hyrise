#pragma once

#include <memory>
#include <vector>

#include "all_parameter_variant.hpp"
#include "scheduler/operator_task.hpp"

namespace opossum {

// Query plan that is generated by the SQLQueryTranslator.
// Note: the query plan must be constructed so that the last added task
// is also the final task that will contain the result of the query plan.
class   SQLQueryPlan {
 public:
  explicit SQLQueryPlan(std::vector<std::shared_ptr<OperatorTask>> tasks = {});

  // Returns the current size of the query plan.
  size_t size() const;

  // Returns the task that was most recently added to the plan. This task is the
  // final task in the query plan and contains its result after execution.
  std::shared_ptr<OperatorTask> back() const;

  // Remove the last last from the plan.
  void pop_back();

  // Adds a task to the end of the query plan.
  void add_task(std::shared_ptr<OperatorTask> task);

  // Append all tasks from the other plan to this query plan.
  void append(const SQLQueryPlan& other_plan);

  // Remove all tasks from the current plan.
  void clear();

  // Return the list of tasks in this query plan.
  const std::vector<std::shared_ptr<OperatorTask>>& tasks() const;

  // Recreates the query plan with a new and equivalent set of tasks.
  // The given list of arguments is passed to the recreate method of all operators to replace ValuePlaceholders.
  SQLQueryPlan recreate(const std::vector<AllParameterVariant>& arguments = {}) const;

  // Set the number of parameters that this query plan contains.
  void set_num_parameters(uint16_t num_parameters);

  // Get the number of parameters that this query plan contains.
  uint16_t num_parameters() const;

 protected:
  std::vector<std::shared_ptr<OperatorTask>> _tasks;

  uint16_t _num_parameters;
};

}  // namespace opossum
