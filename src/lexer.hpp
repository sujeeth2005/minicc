// lexer.hpp - lexical analysis: source text -> tokens.
#pragma once
#include <string>
#include <vector>

#include "common.hpp"

namespace minicc {

enum class Tok {
    KwInt, KwReturn, KwIf, KwElse, KwWhile,
    Ident, Number,
    Plus, Minus, Star, Slash, Percent,
    Lt, Gt, Le, Ge, EqEq, Ne,
    Assign, Not, AndAnd, OrOr,
    LParen, RParen, LBrace, RBrace, Semi,
    End
};

struct Token {
    Tok kind = Tok::End;
    std::string text;
    int value = 0;  // numeric value for Tok::Number
    SourceLoc loc;
};

const char* tokName(Tok t);                 // "+", "int", "identifier", ...
std::string describe(const Token& t);       // for error messages
std::vector<Token> lex(const std::string& src);  // throws CompileError

}  // namespace minicc
