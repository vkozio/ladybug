#include "common/exception/exception.h"

#ifdef LBUG_BACKTRACE
#include <cpptrace/cpptrace.hpp>
#endif

namespace lbug {
namespace common {

Exception::Exception(std::string msg) : exception(), exception_message_(std::move(msg)) {
#ifdef LBUG_BACKTRACE
    cpptrace::generate_trace(1 /*skip this function's frame*/).print();
#endif
}

Exception::Exception(std::string msg, bool skipBacktrace)
    : exception(), exception_message_(std::move(msg)) {
#ifdef LBUG_BACKTRACE
    if (!skipBacktrace) {
        cpptrace::generate_trace(1 /*skip this function's frame*/).print();
    }
#endif
}

} // namespace common
} // namespace lbug
