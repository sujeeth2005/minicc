// parser.hpp - recursive-descent parser with precedence climbing for expressions.
#pragma once
#include <memory>
#include <vector>

#include "ast.hpp"

namespace minicc {
std::unique_ptr<Program> parse(const std::vector<Token>& tokens);  // throws CompileError
}
