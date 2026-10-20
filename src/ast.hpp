// ast.hpp - abstract syntax tree.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"
#include "lexer.hpp"

namespace minicc {

// ---- Expressions ---------------------------------------------------------
enum class ExprKind { Number, Var, Unary, Binary };

struct Expr {
    ExprKind kind;
    SourceLoc loc;
    Expr(ExprKind k, SourceLoc l) : kind(k), loc(l) {}
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct NumberExpr : Expr {
    int value;
    NumberExpr(int v, SourceLoc l) : Expr(ExprKind::Number, l), value(v) {}
};

struct VarExpr : Expr {
    std::string name;
    std::string uniq;  // unique name assigned by semantic analysis (handles shadowing)
    VarExpr(std::string n, SourceLoc l) : Expr(ExprKind::Var, l), name(std::move(n)) {}
};

struct UnaryExpr : Expr {
    Tok op;  // Minus or Not
    ExprPtr operand;
    UnaryExpr(Tok o, ExprPtr e, SourceLoc l) : Expr(ExprKind::Unary, l), op(o), operand(std::move(e)) {}
};

struct BinaryExpr : Expr {
    Tok op;
    ExprPtr lhs, rhs;
    BinaryExpr(Tok o, ExprPtr a, ExprPtr b, SourceLoc l)
        : Expr(ExprKind::Binary, l), op(o), lhs(std::move(a)), rhs(std::move(b)) {}
};

// ---- Statements ----------------------------------------------------------
enum class StmtKind { Block, Decl, Assign, If, While, Return };

struct Stmt {
    StmtKind kind;
    SourceLoc loc;
    Stmt(StmtKind k, SourceLoc l) : kind(k), loc(l) {}
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
    explicit BlockStmt(SourceLoc l) : Stmt(StmtKind::Block, l) {}
};

struct DeclStmt : Stmt {
    std::string name, uniq;
    ExprPtr init;  // may be null
    DeclStmt(std::string n, ExprPtr i, SourceLoc l)
        : Stmt(StmtKind::Decl, l), name(std::move(n)), init(std::move(i)) {}
};

struct AssignStmt : Stmt {
    std::string name, uniq;
    ExprPtr value;
    AssignStmt(std::string n, ExprPtr v, SourceLoc l)
        : Stmt(StmtKind::Assign, l), name(std::move(n)), value(std::move(v)) {}
};

struct IfStmt : Stmt {
    ExprPtr cond;
    StmtPtr thenBranch, elseBranch;  // elseBranch may be null
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e, SourceLoc l)
        : Stmt(StmtKind::If, l), cond(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct WhileStmt : Stmt {
    ExprPtr cond;
    StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b, SourceLoc l)
        : Stmt(StmtKind::While, l), cond(std::move(c)), body(std::move(b)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value;
    ReturnStmt(ExprPtr v, SourceLoc l) : Stmt(StmtKind::Return, l), value(std::move(v)) {}
};

struct Program {
    std::string funcName;
    std::unique_ptr<BlockStmt> body;
    SourceLoc loc;
};

std::string dump(const Program& p);  // indented tree, used by --ast and tests

}  // namespace minicc
