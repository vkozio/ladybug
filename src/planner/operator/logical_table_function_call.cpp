#include "planner/operator/logical_table_function_call.h"

namespace lbug {
namespace planner {

void LogicalTableFunctionCall::computeFlatSchema() {
    createEmptySchema();
    auto groupPos = schema->createGroup();
    // Table functions with a known single-row result (e.g. CURRENT_SETTING, DB_VERSION)
    // should be represented as a single-state (flat) group in the logical schema.
    // This prevents factorization from treating their output as an unflat group with
    // DEFAULT_VECTOR_CAPACITY multiplicity, which leads to inflated row counts in
    // result collectors and client APIs (Node, shell, etc.).
    if (bindData->numRows == 1) {
        schema->setGroupAsSingleState(groupPos);
    }
    for (auto& expr : bindData->columns) {
        schema->insertToGroupAndScope(expr, groupPos);
    }
}

void LogicalTableFunctionCall::computeFactorizedSchema() {
    createEmptySchema();
    auto groupPos = schema->createGroup();
    if (bindData->numRows == 1) {
        schema->setGroupAsSingleState(groupPos);
    }
    for (auto& expr : bindData->columns) {
        schema->insertToGroupAndScope(expr, groupPos);
    }
}

std::unique_ptr<OPPrintInfo> LogicalTableFunctionCall::getPrintInfo() const {
    return std::make_unique<LogicalTableFunctionCallPrintInfo>(getExpressionsForPrinting());
}

} // namespace planner
} // namespace lbug
