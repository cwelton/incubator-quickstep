#ifndef QUICKSTEP_QUERY_OPTIMIZER_RULES_SWAP_PROBE_BUILD_HPP_
#define QUICKSTEP_QUERY_OPTIMIZER_RULES_SWAP_PROBE_BUILD_HPP_

#include <string>

#include "query_optimizer/physical/Physical.hpp"
#include "query_optimizer/rules/Rule.hpp"
#include "query_optimizer/rules/BottomUpRule.hpp"
#include "query_optimizer/cost_model/SimpleCostModel.hpp"
#include "utility/Macros.hpp"

namespace quickstep {
namespace optimizer {

/** \addtogroup OptimizerRules
 *  @{
 */

namespace P = ::quickstep::optimizer::physical;
namespace E = ::quickstep::optimizer::expressions;
namespace C = ::quickstep::optimizer::cost;

/**
 * @brief Rule that applies to a physical plan to arrange probe and
 *        build side based on the cardinalities.
 */
class SwapProbeBuild : public BottomUpRule<P::Physical> {
 public:
  SwapProbeBuild() {
  }

  std::string getName() const override { return "SwapProbeBuild"; }

 protected:
  P::PhysicalPtr applyToNode(const P::PhysicalPtr &input) override;
  void init(const P::PhysicalPtr &input) override;

 private:
  std::unique_ptr<C::SimpleCostModel> cost_model_;

  DISALLOW_COPY_AND_ASSIGN(SwapProbeBuild);
};

}  // namespace optimizer
}  // namespace quickstep

#endif
