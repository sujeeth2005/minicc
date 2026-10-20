// optimizer.hpp - IR-to-IR optimization passes.
#pragma once
#include "ir.hpp"

namespace minicc {

struct OptStats {
    int iterations = 0;
    int propagated = 0;      // operands replaced by a known constant / copy
    int folded = 0;          // constant expressions evaluated at compile time
    int simplified = 0;      // algebraic identities (x+0, x*1, x*0, ...)
    int branchesFolded = 0;  // conditional jumps with a constant condition
    int unreachable = 0;     // instructions removed because control can never reach them
    int jumpsRemoved = 0;    // redundant gotos / unused labels
    int deadRemoved = 0;     // dead assignments removed
};

// Runs all passes to a fixed point.
void optimizeIR(IRFunction& f, OptStats* stats = nullptr);

}  // namespace minicc
