#include "parser.hpp"

namespace minicc {
namespace {

// Binary operator precedence (higher binds tighter). 0 = not a binary operator.
// All binary operators are left-associative.
int precedence(Tok k) {
    switch (k) {
        case Tok::OrOr: return 1;
        case Tok::AndAnd: return 2;
        case Tok::EqEq: case Tok::Ne: return 3;
        case Tok::Lt: case Tok::Gt: case Tok::Le: case Tok::Ge: return 4;
        case Tok::Plus: case Tok::Minus: return 5;
        case Tok::Star: case Tok::Slash: case Tok::Percent: return 6;
        default: return 0;
    }
}

class Parser {
public:
    explicit Parser(const std::vector<Token>& toks) : t_(toks) {}

    std::unique_ptr<Program> parseProgram() {
        auto prog = std::make_unique<Program>();
        prog->loc = cur().loc;
        expect(Tok::KwInt, "at the start of a function definition");
        Token name = expect(Tok::Ident, "as the function name");
        prog->funcName = name.text;
        prog->loc = name.loc;
        expect(Tok::LParen, "after the function name");
        expect(Tok::RParen, "after '(' (function parameters are not supported)");
        prog->body = parseBlock();
        if (!at(Tok::End))
            fail("unexpected " + describe(cur()) + " after the end of function '" + prog->funcName + "'");
        return prog;
    }

private:
    const std::vector<Token>& t_;
    size_t p_ = 0;

    const Token& cur() const { return t_[p_]; }
    bool at(Tok k) const { return cur().kind == k; }
    Token advance() {
        Token x = t_[p_];
        if (p_ + 1 < t_.size()) ++p_;
        return x;
    }
    [[noreturn]] void fail(const std::string& msg) const { throw CompileError(cur().loc, msg); }
    Token expect(Tok k, const std::string& ctx) {
        if (!at(k))
            fail(std::string("expected '") + tokName(k) + "' " + ctx + ", but found " + describe(cur()));
        return advance();
    }

    std::unique_ptr<BlockStmt> parseBlock() {
        Token open = expect(Tok::LBrace, "to start a block");
        auto block = std::make_unique<BlockStmt>(open.loc);
        while (!at(Tok::RBrace) && !at(Tok::End)) block->stmts.push_back(parseStatement());
        expect(Tok::RBrace, "to close the block");
        return block;
    }

    // Body of an if/while: any statement except a bare declaration.
    StmtPtr parseBody(const char* what) {
        if (at(Tok::KwInt))
            fail(std::string("a declaration cannot be the direct body of ") + what + "; wrap it in '{ }'");
        return parseStatement();
    }

    StmtPtr parseStatement() {
        switch (cur().kind) {
            case Tok::LBrace: return parseBlock();
            case Tok::KwInt: return parseDecl();
            case Tok::KwIf: return parseIf();
            case Tok::KwWhile: return parseWhile();
            case Tok::KwReturn: return parseReturn();
            case Tok::Ident: return parseAssign();
            default: fail("expected a statement, but found " + describe(cur()));
        }
    }

    StmtPtr parseDecl() {
        advance();  // int
        Token name = expect(Tok::Ident, "after 'int' (variable name)");
        ExprPtr init;
        if (at(Tok::Assign)) {
            advance();
            init = parseExpr();
        }
        expect(Tok::Semi, "after the variable declaration");
        return std::make_unique<DeclStmt>(name.text, std::move(init), name.loc);
    }

    StmtPtr parseAssign() {
        Token name = advance();
        if (!at(Tok::Assign))
            fail("expected '=' after '" + name.text + "' (a statement must be a declaration, assignment, if, while, return or block), but found " + describe(cur()));
        advance();
        ExprPtr value = parseExpr();
        expect(Tok::Semi, "after the assignment");
        return std::make_unique<AssignStmt>(name.text, std::move(value), name.loc);
    }

    StmtPtr parseIf() {
        Token kw = advance();
        expect(Tok::LParen, "after 'if'");
        ExprPtr cond = parseExpr();
        expect(Tok::RParen, "after the if condition");
        StmtPtr thenS = parseBody("an 'if'");
        StmtPtr elseS;
        if (at(Tok::KwElse)) {
            advance();
            elseS = parseBody("an 'else'");
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(thenS), std::move(elseS), kw.loc);
    }

    StmtPtr parseWhile() {
        Token kw = advance();
        expect(Tok::LParen, "after 'while'");
        ExprPtr cond = parseExpr();
        expect(Tok::RParen, "after the while condition");
        StmtPtr body = parseBody("a 'while'");
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body), kw.loc);
    }

    StmtPtr parseReturn() {
        Token kw = advance();
        ExprPtr v = parseExpr();
        expect(Tok::Semi, "after the return statement");
        return std::make_unique<ReturnStmt>(std::move(v), kw.loc);
    }

    // Precedence climbing: parse operators with precedence >= minPrec.
    ExprPtr parseExpr(int minPrec = 1) {
        ExprPtr lhs = parseUnary();
        for (;;) {
            int prec = precedence(cur().kind);
            if (prec < minPrec || prec == 0) break;
            Token op = advance();
            ExprPtr rhs = parseExpr(prec + 1);  // +1 => left associativity
            lhs = std::make_unique<BinaryExpr>(op.kind, std::move(lhs), std::move(rhs), op.loc);
        }
        return lhs;
    }

    ExprPtr parseUnary() {
        if (at(Tok::Minus) || at(Tok::Not)) {
            Token op = advance();
            return std::make_unique<UnaryExpr>(op.kind, parseUnary(), op.loc);
        }
        return parsePrimary();
    }

    ExprPtr parsePrimary() {
        if (at(Tok::Number)) {
            Token n = advance();
            return std::make_unique<NumberExpr>(n.value, n.loc);
        }
        if (at(Tok::Ident)) {
            Token n = advance();
            return std::make_unique<VarExpr>(n.text, n.loc);
        }
        if (at(Tok::LParen)) {
            advance();
            ExprPtr e = parseExpr();
            expect(Tok::RParen, "to close the parenthesized expression");
            return e;
        }
        fail("expected an expression, but found " + describe(cur()));
    }
};

}  // namespace

std::unique_ptr<Program> parse(const std::vector<Token>& tokens) {
    return Parser(tokens).parseProgram();
}

}  // namespace minicc
