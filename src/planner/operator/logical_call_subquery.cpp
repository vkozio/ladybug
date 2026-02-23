#include "planner/operator/logical_call_subquery.h"

#include "planner/operator/factorization/sink_util.h"

namespace lbug {
namespace planner {

Schema* LogicalCallSubquery::getInnerOutputSchema() const {
    auto* inner = getInnerChild().get();
    if (inner->getOperatorType() != LogicalOperatorType::SCOPE_SCAN ||
        inner->getNumChildren() == 0) {
        return inner->getSchema();
    }
    if (!cachedInnerOutputSchema) {
        auto* scopeSchema = inner->getSchema();
        auto* projSchema = inner->getChild(0)->getSchema();
        cachedInnerOutputSchema = scopeSchema->copy();
        SinkOperatorUtil::mergeSchema(*projSchema, projSchema->getExpressionsInScope(),
            *cachedInnerOutputSchema);
    }
    return cachedInnerOutputSchema.get();
}

void LogicalCallSubquery::computeFactorizedSchema() {
    auto outerSchema = getOuterChild()->getSchema();
    auto* innerSchema = getInnerOutputSchema();
    schema = outerSchema->copy();
    binder::expression_vector toMerge;
    for (auto& expr : innerSchema->getExpressionsInScope()) {
        if (!schema->isExpressionInScope(*expr)) {
            toMerge.push_back(expr);
        }
    }
    if (!toMerge.empty()) {
        SinkOperatorUtil::mergeSchema(*innerSchema, toMerge, *schema);
    }
}

void LogicalCallSubquery::computeFlatSchema() {
    auto outerSchema = getOuterChild()->getSchema();
    auto* innerSchema = getInnerOutputSchema();
    schema = outerSchema->copy();
    binder::expression_vector toAdd;
    for (auto& expression : innerSchema->getExpressionsInScope()) {
        if (!schema->isExpressionInScope(*expression)) {
            toAdd.push_back(expression);
        }
    }
    if (!toAdd.empty()) {
        auto innerGroupPos = schema->createGroup();
        for (auto& expression : toAdd) {
            schema->insertToGroupAndScope(expression, innerGroupPos);
        }
    }
}

} // namespace planner
} // namespace lbug
