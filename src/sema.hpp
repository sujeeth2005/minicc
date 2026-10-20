// sema.hpp - semantic analysis: scopes, declarations, simple checks.
#pragma once
#include <vector>

#include "ast.hpp"

namespace minicc {

struct SemaResult {
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> warnings;
};

// Resolves every variable use to a unique name (stored in the AST) so later
// phases never have to think about scopes or shadowing.
SemaResult analyze(Program& prog);

}  // namespace minicc
