#include "query_optimizer/rules/SwapProbeBuild.hpp"

#include <cstddef>
#include <vector>

#include "query_optimizer/cost_model/StarSchemaSimpleCostModel.hpp"
#include "query_optimizer/expressions/AttributeReference.hpp"
#include "query_optimizer/physical/HashJoin.hpp"
#include "query_optimizer/physical/PatternMatcher.hpp"
#include "query_optimizer/physical/Physical.hpp"
#include "query_optimizer/physical/TopLevelPlan.hpp"
#include "query_optimizer/rules/Rule.hpp"


namespace quickstep {
namespace optimizer {

P::PhysicalPtr SwapProbeBuild::applyToNode(const P::PhysicalPtr &input) {
  P::HashJoinPtr hash_join;

  if (P::SomeHashJoin::MatchesWithConditionalCast(input, &hash_join)) {
    P::PhysicalPtr left = hash_join->left();
    P::PhysicalPtr right = hash_join->right();

    P::TopLevelPlanPtr top_level;
    if (P::SomeTopLevelPlan::MatchesWithConditionalCast(input, &top_level)) {
      cost_model_.reset(new C::StarSchemaSimpleCostModel(top_level->shared_subplans()));
    } else {
      std::vector<P::PhysicalPtr> plans = {input};
      cost_model_.reset(new C::StarSchemaSimpleCostModel(plans));
    }

    std::size_t left_cardinality = cost_model_->estimateCardinality(left);
    std::size_t right_cardinality = cost_model_->estimateCardinality(right);

    if (right_cardinality > left_cardinality) {
      std::vector<E::AttributeReferencePtr> left_join_attributes = hash_join->left_join_attributes();
      std::vector<E::AttributeReferencePtr> right_join_attributes = hash_join->right_join_attributes();

      P::PhysicalPtr output = P::HashJoin::Create(right,
                                                  left,
                                                  right_join_attributes,
                                                  left_join_attributes,
                                                  hash_join->residual_predicate(),
                                                  hash_join->project_expressions(),
                                                  hash_join->join_type(),
                                                  left_cardinality);
      LOG_APPLYING_RULE(input, output);
      return output;
    }
    else {
      P::PhysicalPtr output = P::HashJoin::Create(left,
                                                  right,
                                                  hash_join->left_join_attributes(),
                                                  hash_join->right_join_attributes(),
                                                  hash_join->residual_predicate(),
                                                  hash_join->project_expressions(),
                                                  hash_join->join_type(),
                                                  right_cardinality);
      // Since we did not apply the swap logic, we will not report it to the log.
      // However we also did not ignored the rule completely, therefore we will not
      // log that we ignored the rule.
      return output;
    }
  }

  LOG_IGNORING_RULE(input);
  return input;
}

}  // namespace optimizer
}  // namespace quickstep
