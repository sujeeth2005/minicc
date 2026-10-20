#include "sema.hpp"

#include <unordered_map>

namespace minicc {
namespace {

struct VarInfo {
    std::string uniq;
    int declLine;
};

class Sema {
public:
    SemaResult run(Program& prog) {
        if (prog.funcName != "main")
            error(prog.loc, "the program must define 'int main()', found '" + prog.funcName + "'");
        stmt(*prog.body);
        if (!alwaysReturns(*prog.body))
            warn(prog.loc, "control may reach the end of '" + prog.funcName + "' without a return; 0 is returned");
        return std::move(res_);
    }

private:
    SemaResult res_;
    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
    std::unordered_map<std::string, int> counts_;

    void error(SourceLoc l, std::string m) { res_.errors.push_back({l, std::move(m)}); }
    void warn(SourceLoc l, std::string m) { res_.warnings.push_back({l, std::move(m)}); }

    // First "x" stays "x"; later declarations of the same name become "x.1", "x.2", ...
    std::string makeUnique(const std::string& name) {
        int n = counts_[name]++;
        std::string base = isTempName(name) ? name + "_" : name;  // never collide with temporaries
        return n == 0 ? base : base + "." + std::to_string(n);
    }

    const VarInfo* lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto f = it->find(name);
            if (f != it->end()) return &f->second;
        }
        return nullptr;
    }

    void expr(Expr& e) {
        switch (e.kind) {
            case ExprKind::Number: break;
            case ExprKind::Var: {
                auto& v = static_cast<VarExpr&>(e);
                if (const VarInfo* info = lookup(v.name)) v.uniq = info->uniq;
                else error(v.loc, "use of undeclared variable '" + v.name + "'");
                break;
            }
            case ExprKind::Unary:
                expr(*static_cast<UnaryExpr&>(e).operand);
                break;
            case ExprKind::Binary: {
                auto& b = static_cast<BinaryExpr&>(e);
                expr(*b.lhs);
                expr(*b.rhs);
                if ((b.op == Tok::Slash || b.op == Tok::Percent) && b.rhs->kind == ExprKind::Number &&
                    static_cast<NumberExpr&>(*b.rhs).value == 0)
                    error(b.loc, b.op == Tok::Slash ? "division by zero" : "remainder by zero");
                break;
            }
        }
    }

    void stmt(Stmt& s) {
        switch (s.kind) {
            case StmtKind::Block: {
                scopes_.emplace_back();
                for (auto& c : static_cast<BlockStmt&>(s).stmts) stmt(*c);
                scopes_.pop_back();
                break;
            }
            case StmtKind::Decl: {
                auto& d = static_cast<DeclStmt&>(s);
                if (d.init) expr(*d.init);  // analyzed before the name is in scope: "int x = x;" is an error
                auto& scope = scopes_.back();
                auto it = scope.find(d.name);
                if (it != scope.end()) {
                    error(d.loc, "redeclaration of '" + d.name + "' (first declared on line " +
                                     std::to_string(it->second.declLine) + ")");
                    d.uniq = it->second.uniq;
                } else {
                    d.uniq = makeUnique(d.name);
                    scope[d.name] = {d.uniq, d.loc.line};
                }
                break;
            }
            case StmtKind::Assign: {
                auto& a = static_cast<AssignStmt&>(s);
                expr(*a.value);
                if (const VarInfo* info = lookup(a.name)) a.uniq = info->uniq;
                else error(a.loc, "assignment to undeclared variable '" + a.name + "'");
                break;
            }
            case StmtKind::If: {
                auto& i = static_cast<IfStmt&>(s);
                expr(*i.cond);
                stmt(*i.thenBranch);
                if (i.elseBranch) stmt(*i.elseBranch);
                break;
            }
            case StmtKind::While: {
                auto& w = static_cast<WhileStmt&>(s);
                expr(*w.cond);
                stmt(*w.body);
                break;
            }
            case StmtKind::Return:
                expr(*static_cast<ReturnStmt&>(s).value);
                break;
        }
    }

    // Conservative "does every path return?" check (loops are assumed to be able to exit).
    static bool alwaysReturns(const Stmt& s) {
        switch (s.kind) {
            case StmtKind::Return: return true;
            case StmtKind::Block:
                for (auto& c : static_cast<const BlockStmt&>(s).stmts)
                    if (alwaysReturns(*c)) return true;
                return false;
            case StmtKind::If: {
                auto& i = static_cast<const IfStmt&>(s);
                return i.elseBranch && alwaysReturns(*i.thenBranch) && alwaysReturns(*i.elseBranch);
            }
            default: return false;
        }
    }
};

}  // namespace

SemaResult analyze(Program& prog) { return Sema().run(prog); }

}  // namespace minicc
