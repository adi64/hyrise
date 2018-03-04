#pragma once

#include <memory>
#include <set>

#include "tuning/tuning_operation.hpp"

namespace opossum {

/**
 * A TuningChoice represents a possible system modification with a certain
 * performance impact coming at a cost.
 *
 * TuningChoices are generated by AbstractEvaluators in a Tuner instance
 * and either accepted or rejected by an AbstractSelector while generating
 * a sequence of TuningOperations to improve the overall system performance.
 */
class TuningChoice {
 public:
  virtual ~TuningChoice() {}
  /**
   * An estimate regarding the performance impact of this modification.
   * values < 0.0f: expected degradation of system performance
   * values > 0.0f: expected improvement of system performance
   *
   * No scaling constraints are specified so that this value is only comparable
   * among TuningChoices generated by compatible AbstractEvaluator implementations.
   */
  virtual float desirability() const = 0;

  /**
   * How certain the producing evaluator was when generating this choice.
   * Very basic evaluators will probably output choices with low confidence
   * in general.
   * Similarly, if an evaluator is very specialized for a certain edge case,
   * it might produce few choices with high confidence and several other
   * choices with a low confidence value, as other evaluators might be able
   * to give better results in these cases.
   */
  virtual float confidence() const = 0;

  /**
   * Convenience accessors for desirability() that in their default implementation
   * return the performance benetfts expected from accepting or rejecting this choice.
   */
  virtual float accept_desirability() const;
  virtual float reject_desirability() const;

  /**
   * An estimate of the absolute costs of this modification.
   *
   * As this value is intended to be counted against a common budget,
   * care must be taken that all AbstractEvaluators use the same cost measure.
   */
  virtual float cost() const = 0;

  /**
   * Convenience accessors for cost() that in their default implementation
   * return the cost this choice currently imposes on the system and the respective
   * cost delta resulting from an accept and reject operation.
   */
  virtual float current_cost() const;
  virtual float accept_cost() const;
  virtual float reject_cost() const;

  /**
   * True, if this modification is already present in the current system state.
   */
  virtual bool is_currently_chosen() const = 0;

  /**
   * A list of other TuningChoices that should/can not be chosen if this
   * TuningChoice is accepted.
   */
  const std::set<std::shared_ptr<TuningChoice>>& invalidates() const;

  /**
   * Add a TuningChoice that should/can not be chosen if this TuningChoice
   * is accepted.
   */
  void add_invalidate(std::shared_ptr<TuningChoice> choice);

  /**
   * Get a TuningOperation that causes this modification to be present
   * in the current system state.
   *
   * The default implementation returns the _accept_operation() if
   * is_currently_chosen() is false, else a NullOperation
   */
  virtual std::shared_ptr<TuningOperation> accept() const;

  /**
   * Get a TuningOperation that causes this modification not to be present
   * in the current system state.
   *
   * The default implementation returns the _reject_operation() if
   * is_currently_chosen() is true, else a NullOperation
   */
  virtual std::shared_ptr<TuningOperation> reject() const;

  /**
   * Print detailed information on the concrete TuningChoice.
   *
   * The default implementation prints the information reported by
   * desirability(), cost() and isCurrentlyChosen().
   */
  virtual void print_on(std::ostream& output) const;

  friend std::ostream& operator<<(std::ostream& output, const TuningChoice& choice) {
    choice.print_on(output);
    return output;
  }

 protected:
  /**
   * Create a TuningOperation that performs this modification of the system state.
   */
  virtual std::shared_ptr<TuningOperation> _accept_operation() const = 0;

  /**
   * Create a TuningOperation that reverts this modification of the system state.
   */
  virtual std::shared_ptr<TuningOperation> _reject_operation() const = 0;

  std::set<std::shared_ptr<TuningChoice>> _invalidates;
};

}  // namespace opossum
