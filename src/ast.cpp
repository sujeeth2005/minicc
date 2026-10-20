#include "ast.hpp"

#include <sstream>

namespace minicc {
namespace {

void dumpExpr(std::ostream& os, const Expr& e, int ind) {
    std::string pad(static_cast<size_t>(ind), ' ');
    switch (e.kind) {
        case ExprKind::Number:
            os << pad << "Number " << static_cast<const NumberExpr&>(e).value << '\n';
            break;
        case ExprKind::Var:
            os << pad << "Var " << static_cast<const VarExpr&>(e).name << '\n';
            break;
        case ExprKind::Unary: {
            auto& u = static_cast<const UnaryExpr&>(e);
            os << pad << "Unary " << tokName(u.op) << '\n';
            dumpExpr(os, *u.operand, ind + 2);
            break;
        }
        case ExprKind::Binary: {
            auto& b = static_cast<const BinaryExpr&>(e);
            os << pad << "Binary " << tokName(b.op) << '\n';
            dumpExpr(os, *b.lhs, ind + 2);
            dumpExpr(os, *b.rhs, ind + 2);
            break;
        }
    }
}

void dumpStmt(std::ostream& os, const Stmt& s, int ind) {
    std::string pad(static_cast<size_t>(ind), ' ');
    switch (s.kind) {
        case StmtKind::Block:
            os << pad << "Block\n";
            for (auto& c : static_cast<const BlockStmt&>(s).stmts) dumpStmt(os, *c, ind + 2);
            break;
        case StmtKind::Decl: {
            auto& d = static_cast<const DeclStmt&>(s);
            os << pad << "Decl " << d.name << '\n';
            if (d.init) dumpExpr(os, *d.init, ind + 2);
            break;
        }
        case StmtKind::Assign: {
            auto& a = static_cast<const AssignStmt&>(s);
            os << pad << "Assign " << a.name << '\n';
            dumpExpr(os, *a.value, ind + 2);
            break;
        }
        case StmtKind::If: {
            auto& i = static_cast<const IfStmt&>(s);
            os << pad << "If\n" << pad << "  cond:\n";
            dumpExpr(os, *i.cond, ind + 4);
            os << pad << "  then:\n";
            dumpStmt(os, *i.thenBranch, ind + 4);
            if (i.elseBranch) {
                os << pad << "  else:\n";
                dumpStmt(os, *i.elseBranch, ind + 4);
            }
            break;
        }
        case StmtKind::While: {
            auto& w = static_cast<const WhileStmt&>(s);
            os << pad << "While\n" << pad << "  cond:\n";
            dumpExpr(os, *w.cond, ind + 4);
            os << pad << "  body:\n";
            dumpStmt(os, *w.body, ind + 4);
            break;
        }
        case StmtKind::Return:
            os << pad << "Return\n";
            dumpExpr(os, *static_cast<const ReturnStmt&>(s).value, ind + 2);
            break;
    }
}

}  // namespace

std::string dump(const Program& p) {
    std::ostringstream os;
    os << "Program " << p.funcName << '\n';
    dumpStmt(os, *p.body, 2);
    return os.str();
}

}  // namespace minicc
