#include "binder/expression/node_rel_expression.h"
#include "common/enums/expression_type.h"
#include "common/exception/internal.h"
#include "planner/operator/logical_call_subquery.h"
#include "processor/operator/call_subquery.h"
#include "processor/plan_mapper.h"
#include "processor/result/result_set_descriptor.h"

using namespace lbug::common;
using namespace lbug::planner;

namespace lbug {
namespace processor {

namespace {

// Resolve scope expression to DataPos in outer schema. Scope expressions from the binder
// may have a different uniqueName than the same variable in the outer plan's schema (e.g. if
// the outer schema was built from a copy). Fall back to matching by alias.
std::vector<DataPos> getScopeDataPositions(const binder::expression_vector& scopeExpressions,
    const Schema& outerSchema) {
    std::vector<DataPos> result;
    result.reserve(scopeExpressions.size());
    for (auto& scopeExpr : scopeExpressions) {
        if (outerSchema.containsExpression(scopeExpr->getUniqueName())) {
            result.push_back(PlanMapper::getDataPos(*scopeExpr, outerSchema));
            continue;
        }
        // Fallback: same variable may have different uniqueName in outer schema (e.g. after plan
        // copy). Match by same expression object, then by alias, then by uniqueName suffix (e.g.
        // _0_a -> a).
        bool found = false;
        for (auto& inScope : outerSchema.getExpressionsInScope()) {
            if (inScope.get() == scopeExpr.get()) {
                result.push_back(PlanMapper::getDataPos(*inScope, outerSchema));
                found = true;
                break;
            }
        }
        if (!found) {
            const std::string& alias = scopeExpr->getAlias();
            for (auto& inScope : outerSchema.getExpressionsInScope()) {
                if (!inScope->getAlias().empty() && inScope->getAlias() == alias) {
                    result.push_back(PlanMapper::getDataPos(*inScope, outerSchema));
                    found = true;
                    break;
                }
            }
        }
        // Fallback: match by variable name suffix from uniqueName (e.g. _0_a -> a; binder may use
        // different id in outer plan).
        if (!found) {
            const std::string& scopeUnique = scopeExpr->getUniqueName();
            auto lastUnderscore = scopeUnique.rfind('_');
            std::string scopeVarSuffix =
                (lastUnderscore != std::string::npos && lastUnderscore + 1 < scopeUnique.size()) ?
                    scopeUnique.substr(lastUnderscore + 1) :
                    scopeUnique;
            for (auto& inScope : outerSchema.getExpressionsInScope()) {
                const std::string& inUnique = inScope->getUniqueName();
                auto inLast = inUnique.rfind('_');
                std::string inVarSuffix =
                    (inLast != std::string::npos && inLast + 1 < inUnique.size()) ?
                        inUnique.substr(inLast + 1) :
                        inUnique;
                if (inVarSuffix == scopeVarSuffix) {
                    result.push_back(PlanMapper::getDataPos(*inScope, outerSchema));
                    found = true;
                    break;
                }
            }
        }
        // Fallback: scope has Node/Rel variable (e.g. "a"); outer schema has internal ID (Scan
        // stores nodeID = node.getInternalID()). Resolve by matching internal ID in outer schema.
        if (!found && scopeExpr->expressionType == ExpressionType::PATTERN) {
            auto& nodeOrRel = scopeExpr->constCast<binder::NodeOrRelExpression>();
            std::shared_ptr<binder::Expression> internalID = nodeOrRel.getInternalID();
            if (outerSchema.containsExpression(internalID->getUniqueName())) {
                result.push_back(PlanMapper::getDataPos(*internalID, outerSchema));
                found = true;
            }
            if (!found) {
                for (auto& inScope : outerSchema.getExpressionsInScope()) {
                    if (inScope->getUniqueName() == internalID->getUniqueName()) {
                        result.push_back(PlanMapper::getDataPos(*inScope, outerSchema));
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) {
            throw InternalException(std::format(
                "CALL subquery: scope variable '{}' (uniqueName '{}') not found in outer schema.",
                scopeExpr->getAlias(), scopeExpr->getUniqueName()));
        }
    }
    return result;
}

} // namespace

std::unique_ptr<PhysicalOperator> PlanMapper::mapCallSubquery(
    const LogicalOperator* logicalOperator) {
    auto& logicalCall = logicalOperator->constCast<LogicalCallSubquery>();
    // Use schema from the first operator that has scope expressions (e.g. Scan). Filter may
    // not have all names in its schema depending on how it was built.
    const planner::LogicalOperator* outerOp = logicalCall.getOuterChild().get();
    while (outerOp->getOperatorType() == LogicalOperatorType::FILTER &&
           outerOp->getNumChildren() > 0) {
        outerOp = outerOp->getChild(0).get();
    }
    auto* outerSchema = outerOp->getSchema();
    auto* innerOutputSchema = logicalCall.getInnerOutputSchema();
    auto scopeOutPos = getScopeDataPositions(logicalCall.getScopeExpressions(), *outerSchema);
    std::vector<DataPos> innerOutputPositions;
    std::vector<DataPos> innerReadPositions;
    auto numOuterGroups = outerSchema->getNumGroups();
    const data_chunk_pos_t scopeChunkOffset = scopeOutPos.empty() ? 0 : 1;
    const planner::Schema* consumerSchema = getResultSetSchema();
    for (auto& expr : innerOutputSchema->getExpressionsInScope()) {
        if (consumerSchema && consumerSchema->containsExpression(expr->getUniqueName())) {
            innerOutputPositions.push_back(PlanMapper::getDataPos(*expr, *consumerSchema));
        } else {
            auto [groupPos, valuePos] = innerOutputSchema->getExpressionPos(*expr);
            innerOutputPositions.emplace_back(numOuterGroups + groupPos, valuePos);
        }
        auto [groupPos, valuePos] = innerOutputSchema->getExpressionPos(*expr);
        innerReadPositions.emplace_back(scopeChunkOffset + groupPos, valuePos);
    }
    auto innerDescriptor = std::make_unique<ResultSetDescriptor>(innerOutputSchema);
    CallSubqueryInfo info(std::move(scopeOutPos), std::move(innerOutputPositions),
        std::move(innerReadPositions), numOuterGroups, std::move(innerDescriptor));
    auto outerChild = mapOperator(logicalCall.getOuterChild().get());
    auto innerChild = mapOperator(logicalCall.getInnerChild().get());
    auto printInfo = std::make_unique<OPPrintInfo>();
    return std::make_unique<CallSubquery>(std::move(info), std::move(outerChild),
        std::move(innerChild), getOperatorID(), std::move(printInfo));
}

} // namespace processor
} // namespace lbug
