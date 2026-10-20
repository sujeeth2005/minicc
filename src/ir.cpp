#include "ir.hpp"

#include <climits>
#include <cstdint>
#include <sstream>

namespace minicc {

// ---------------------------------------------------------------------------
// Constant evaluation
// ---------------------------------------------------------------------------
bool evalBinary(Op op, int a, int b, int& out) {
    uint32_t ua = static_cast<uint32_t>(a), ub = static_cast<uint32_t>(b);
    switch (op) {
        case Op::Add: out = static_cast<int>(ua + ub); return true;
        case Op::Sub: out = static_cast<int>(ua - ub); return true;
        case Op::Mul: out = static_cast<int>(ua * ub); return true;
        case Op::Div:
            if (b == 0) return false;
            out = (a == INT_MIN && b == -1) ? INT_MIN : a / b;
            return true;
        case Op::Rem:
            if (b == 0) return false;
            out = (a == INT_MIN && b == -1) ? 0 : a % b;
            return true;
        case Op::Lt: out = a < b; return true;
        case Op::Le: out = a <= b; return true;
        case Op::Gt: out = a > b; return true;
        case Op::Ge: out = a >= b; return true;
        case Op::Eq: out = a == b; return true;
        case Op::Ne: out = a != b; return true;
        default: return false;
    }
}

int evalUnary(Op op, int a) {
    if (op == Op::Neg) return static_cast<int>(0u - static_cast<uint32_t>(a));
    return a == 0 ? 1 : 0;  // Op::Not
}

// ---------------------------------------------------------------------------
// Printing
// ---------------------------------------------------------------------------
const char* opSymbol(Op op) {
    switch (op) {
        case Op::Add: return "+";
        case Op::Sub: return "-";
        case Op::Mul: return "*";
        case Op::Div: return "/";
        case Op::Rem: return "%";
        case Op::Lt: return "<";
        case Op::Le: return "<=";
        case Op::Gt: return ">";
        case Op::Ge: return ">=";
        case Op::Eq: return "==";
        case Op::Ne: return "!=";
        case Op::Neg: return "-";
        case Op::Not: return "!";
        default: return "?";
    }
}

std::string toString(const Instr& i) {
    switch (i.op) {
        case Op::Copy: return i.dst + " = " + i.a.str();
        case Op::Neg:
        case Op::Not: return i.dst + " = " + opSymbol(i.op) + i.a.str();
        case Op::Label: return i.label + ":";
        case Op::Jmp: return "goto " + i.label;
        case Op::Jz: return "ifz " + i.a.str() + " goto " + i.label;
        case Op::Jnz: return "ifnz " + i.a.str() + " goto " + i.label;
        case Op::Ret: return "return " + i.a.str();
        default: return i.dst + " = " + i.a.str() + " " + opSymbol(i.op) + " " + i.b.str();
    }
}

std::string toString(const IRFunction& f) {
    std::ostringstream os;
    for (const Instr& i : f.code) {
        if (i.op != Op::Label) os << "    ";
        os << toString(i) << '\n';
    }
    return os.str();
}

// ---------------------------------------------------------------------------
// AST -> TAC
// ---------------------------------------------------------------------------
namespace {

Op binOp(Tok t) {
    switch (t) {
        case Tok::Plus: return Op::Add;
        case Tok::Minus: return Op::Sub;
        case Tok::Star: return Op::Mul;
        case Tok::Slash: return Op::Div;
        case Tok::Percent: return Op::Rem;
        case Tok::Lt: return Op::Lt;
        case Tok::Le: return Op::Le;
        case Tok::Gt: return Op::Gt;
        case Tok::Ge: return Op::Ge;
        case Tok::EqEq: return Op::Eq;
        default: return Op::Ne;  // Tok::Ne
    }
}

class IRGen {
public:
    IRFunction run(const Program& p) {
        f_.name = p.funcName;
        stmt(*p.body);
        emit(Op::Ret, "", Operand::imm(0));  // implicit "return 0" if control falls off the end
        return std::move(f_);
    }

private:
    IRFunction f_;
    int temps_ = 0, labels_ = 0;

    Operand newTemp() { return Operand::var("t" + std::to_string(++temps_)); }
    std::string newLabel() { return "L" + std::to_string(++labels_); }

    void emit(Op op, std::string dst = "", Operand a = {}, Operand b = {}, std::string label = "") {
        Instr i;
        i.op = op;
        i.dst = std::move(dst);
        i.a = std::move(a);
        i.b = std::move(b);
        i.label = std::move(label);
        f_.code.push_back(std::move(i));
    }
    void label(const std::string& l) { emit(Op::Label, "", {}, {}, l); }
    void jump(Op op, Operand cond, const std::string& l) { emit(op, "", std::move(cond), {}, l); }

    // Short-circuit && and ||: the right operand is only evaluated when needed.
    Operand logical(const BinaryExpr& b) {
        bool isAnd = b.op == Tok::AndAnd;
        Operand result = newTemp();
        std::string shortLbl = newLabel(), endLbl = newLabel();
        Operand l = expr(*b.lhs);
        jump(isAnd ? Op::Jz : Op::Jnz, l, shortLbl);
        Operand r = expr(*b.rhs);
        jump(isAnd ? Op::Jz : Op::Jnz, r, shortLbl);
        emit(Op::Copy, result.name, Operand::imm(isAnd ? 1 : 0));
        jump(Op::Jmp, {}, endLbl);
        label(shortLbl);
        emit(Op::Copy, result.name, Operand::imm(isAnd ? 0 : 1));
        label(endLbl);
        return result;
    }

    Operand expr(const Expr& e) {
        switch (e.kind) {
            case ExprKind::Number: return Operand::imm(static_cast<const NumberExpr&>(e).value);
            case ExprKind::Var: return Operand::var(static_cast<const VarExpr&>(e).uniq);
            case ExprKind::Unary: {
                auto& u = static_cast<const UnaryExpr&>(e);
                Operand a = expr(*u.operand);
                Operand t = newTemp();
                emit(u.op == Tok::Minus ? Op::Neg : Op::Not, t.name, a);
                return t;
            }
            case ExprKind::Binary: {
                auto& b = static_cast<const BinaryExpr&>(e);
                if (b.op == Tok::AndAnd || b.op == Tok::OrOr) return logical(b);
                Operand l = expr(*b.lhs);
                Operand r = expr(*b.rhs);
                Operand t = newTemp();
                emit(binOp(b.op), t.name, l, r);
                return t;
            }
        }
        return Operand::imm(0);
    }

    void stmt(const Stmt& s) {
        switch (s.kind) {
            case StmtKind::Block:
                for (auto& c : static_cast<const BlockStmt&>(s).stmts) stmt(*c);
                break;
            case StmtKind::Decl: {
                auto& d = static_cast<const DeclStmt&>(s);
                // Uninitialized variables are defined to be zero.
                emit(Op::Copy, d.uniq, d.init ? expr(*d.init) : Operand::imm(0));
                break;
            }
            case StmtKind::Assign: {
                auto& a = static_cast<const AssignStmt&>(s);
                emit(Op::Copy, a.uniq, expr(*a.value));
                break;
            }
            case StmtKind::If: {
                auto& i = static_cast<const IfStmt&>(s);
                std::string elseLbl = newLabel();
                jump(Op::Jz, expr(*i.cond), elseLbl);
                stmt(*i.thenBranch);
                if (i.elseBranch) {
                    std::string endLbl = newLabel();
                    jump(Op::Jmp, {}, endLbl);
                    label(elseLbl);
                    stmt(*i.elseBranch);
                    label(endLbl);
                } else {
                    label(elseLbl);
                }
                break;
            }
            case StmtKind::While: {
                auto& w = static_cast<const WhileStmt&>(s);
                std::string top = newLabel(), end = newLabel();
                label(top);
                jump(Op::Jz, expr(*w.cond), end);
                stmt(*w.body);
                jump(Op::Jmp, {}, top);
                label(end);
                break;
            }
            case StmtKind::Return:
                emit(Op::Ret, "", expr(*static_cast<const ReturnStmt&>(s).value));
                break;
        }
    }
};

}  // namespace

IRFunction generateIR(const Program& prog) { return IRGen().run(prog); }

}  // namespace minicc
