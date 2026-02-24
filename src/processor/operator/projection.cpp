#include "processor/operator/projection.h"

#include "binder/expression/expression_util.h"
#include "processor/execution_context.h"

using namespace lbug::evaluator;

namespace lbug {
namespace processor {

std::string ProjectionPrintInfo::toString() const {
    std::string result = "Expressions: ";
    result += binder::ExpressionUtil::toString(expressions);
    return result;
}

void Projection::initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) {
    for (auto i = 0u; i < info.evaluators.size(); ++i) {
        auto& expressionEvaluator = *info.evaluators[i];
        expressionEvaluator.init(*resultSet, context->clientContext);
        auto [dataChunkPos, vectorPos] = info.exprsOutputPos[i];
        auto dataChunk = resultSet->dataChunks[dataChunkPos];
        dataChunk->valueVectors[vectorPos] = expressionEvaluator.resultVector;
    }
}

bool Projection::getNextTuplesInternal(ExecutionContext* context) {
    restoreMultiplicity();
    if (!children[0]->getNextTuple(context)) {
        return false;
    }
    saveMultiplicity();
    for (auto& evaluator : info.evaluators) {
        evaluator->evaluate();
    }
    if (!info.discardedChunkIndices.empty()) {
        auto discardedCount =
            resultSet->getNumTuplesWithoutMultiplicity(info.discardedChunkIndices);
        bool shouldScaleMultiplicity = true;
        // For degenerate constant projections (e.g. RETURN 1, single-row simple table
        // functions), the payload chunks are flat single-row while discarded chunks come
        // from dummy scan or similar bookkeeping groups. In that case we must not inflate
        // multiplicity based on discarded groups.
        if (resultSet->multiplicity == 1 && discardedCount > 1) {
            bool allActiveFlatSingleRow = true;
            for (auto dataChunkPos : info.activeChunkIndices) {
                auto chunk = resultSet->getDataChunk(dataChunkPos);
                if (!chunk || !chunk->state->isFlat() ||
                    chunk->state->getSelVector().getSelSize() != 1) {
                    allActiveFlatSingleRow = false;
                    break;
                }
            }
            if (allActiveFlatSingleRow) {
                shouldScaleMultiplicity = false;
            }
        }
        if (shouldScaleMultiplicity) {
            resultSet->multiplicity *= discardedCount;
        }
    }
    // The if statement is added to avoid the cost of calculating numTuples when metric is disabled.
    if (metrics->numOutputTuple.enabled) [[unlikely]] {
        if (info.activeChunkIndices.empty()) {
            // In COUNT(*) case we are projecting away everything and only track multiplicity
            metrics->numOutputTuple.increase(resultSet->multiplicity);
        } else {
            metrics->numOutputTuple.increase(resultSet->getNumTuples(info.activeChunkIndices));
        }
    }
    return true;
}

} // namespace processor
} // namespace lbug
