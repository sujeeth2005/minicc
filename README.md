# minicc — a mini C compiler in C++17

[![CI](https://github.com/sujeeth2005/minicc/actions/workflows/ci.yml/badge.svg)](https://github.com/sujeeth2005/minicc/actions)

A small, readable compiler for a C-like language. It goes all the way from source text to
assembly for a virtual-register machine, and ships with an interpreter for that assembly so
every stage can be tested end to end.

```
source ─► Lexer ─► Parser ─► Semantic ─► IR gen ─► Optimizer ─► Code gen ─► asm ─► VM
          tokens    AST      analysis    3-address   TAC→TAC    registers
```

## Quick start

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure       # run the test suite

./build/minicc examples/arithmetic.mc --ir --opt-ir --asm --run
```
(On Windows with Visual Studio the binary is `build\Release\minicc.exe`.)

## The language

One type (`int`, 32-bit, wrap-around), one function (`int main()`).

```
program   := "int" "main" "(" ")" block
block     := "{" statement* "}"
statement := "int" IDENT [ "=" expr ] ";"
           | IDENT "=" expr ";"
           | "if" "(" expr ")" statement [ "else" statement ]
           | "while" "(" expr ")" statement
           | "return" expr ";"
           | block
expr      := unary-expr with binary operators (lowest → highest precedence):
             ||   &&   == !=   < > <= >=   + -   * / %
unary     := ("-" | "!") unary | NUMBER | IDENT | "(" expr ")"
```
`//` and `/* */` comments are supported. `&&` / `||` short-circuit. Variables declared without an
initializer are zero. Falling off the end of `main` returns 0 (with a warning).

## Pipeline, stage by stage

| Stage | File | What it does |
|-------|------|--------------|
| Lexer | `src/lexer.cpp` | Characters → tokens with line/column. Handles comments, rejects bad characters and oversized literals. |
| Parser | `src/parser.cpp` | Recursive descent; expressions use **precedence climbing** (one function, a precedence table, left associativity via `prec + 1`). Builds the AST. |
| Semantic analysis | `src/sema.cpp` | Scoped symbol table; undeclared variables, redeclarations, constant division by zero, missing-return warning. Renames shadowed variables (`x`, `x.1`, …) so later stages need no scope logic. Collects *all* errors. |
| IR | `src/ir.cpp` | AST → three-address code with temporaries (`t1`, `t2`, …) and labels. Lowers `if`/`while`/`&&`/`||` to jumps. |
| Optimizer | `src/optimizer.cpp` | See below. Runs to a fixed point. |
| Code generator | `src/codegen.cpp` | IR → virtual-register assembly ([docs/ISA.md](docs/ISA.md)); includes a VM to execute it. |

### Optimizations

* **Constant propagation + folding** — per basic block, so loops are handled soundly.
* **Copy propagation** — `y = x; z = y + 1` becomes `z = x + 1`.
* **Algebraic simplification** — `x+0`, `x-0`, `x*1`, `x*0`, `x/1`.
* **Branch folding** — `ifz 1 goto L` is resolved at compile time.
* **Unreachable-code elimination** — CFG reachability from the entry.
* **Jump cleanup** — removes `goto L` directly before `L:` and unused labels.
* **Temp forwarding** — `t = a op b; x = t` becomes `x = a op b`.
* **Dead-code elimination** — removes pure instructions whose result is never read. Division/remainder
  that could trap are kept unless the divisor is a non-zero constant.

### Example

```c
int main() {
    int x = 10;
    int y = 20;
    int result = x + y * 2;
    return result;
}
```

```
$ minicc examples/arithmetic.mc --ir --opt-ir --run
; --- three-address code (unoptimized) ---
    x = 10
    y = 20
    t1 = y * 2
    t2 = x + t1
    result = t2
    return result
    return 0
; --- three-address code (optimized) ---
    return 50
result: 50  (1 VM steps)
```

A loop is *not* folded, because its values change at run time:

```
$ minicc examples/sum_loop.mc --opt-ir --asm --run
    sum = 0                          mov   r0, 0
    i = 1                            mov   r1, 1
L1:                                L1:
    t1 = i <= 10                     setle r2, r1, 10
    ifz t1 goto L2                   brz   r2, L2
    sum = sum + i                    add   r0, r0, r1
    i = i + 1                        add   r1, r1, 1
    goto L1                          bra   L1
L2:                                L2:
    return sum                       ret   r0
result: 55  (67 VM steps)
```

### Diagnostics

```
bad.mc:3:9: error: redeclaration of 'x' (first declared on line 2)
      int x = 2;
          ^
bad.mc:4:12: error: use of undeclared variable 'y'
      return y + x;
             ^
```

## Command line

```
minicc <file.mc> [-O0|-O1] [--tokens] [--ast] [--ir] [--opt-ir] [--asm] [--stats] [--run]
```

## Testing

`tests/test_main.cpp` is a dependency-free test runner (also registered with CTest):

* lexer token streams and error cases
* operator precedence and associativity (AST shape **and** evaluated results)
* syntax errors, semantic errors, scoping and shadowing
* exact expected IR before and after optimization
* code generation and VM behaviour (traps, infinite-loop guard)
* every program in `examples/` (each declares its expected result in a `// expect:` header)
* **differential testing**: 1000 randomly generated programs must return the same value at `-O0`
  and `-O1`. This is the main tool for catching optimizer bugs.

CI builds on Linux, Windows and macOS and runs the suite again under AddressSanitizer + UBSan.

## Debugging generated code

1. Run `--tokens`, `--ast`, `--ir` to find the first stage whose output is wrong.
2. Compare `-O0` and `-O1` results; if they differ, the optimizer is at fault. Use `--opt-ir` and `--stats`.
3. Reduce the program, then add it to `examples/` or the tests.
4. Use GDB/VS Code (`.vscode/launch.json` is included) or build with `-DMINICC_SANITIZE=ON`.

## Limitations and ideas for extension

* Only `int`; no functions/calls, arrays, pointers or I/O. Type checking is therefore trivial.
* Constant propagation is per basic block; a global data-flow analysis (reaching definitions / liveness)
  would find more, including dead cycles such as a variable only used to update itself.
* No register allocation (unbounded virtual registers) and no real-hardware target yet.
* Next steps: SSA form, common-subexpression elimination, loop-invariant code motion, linear-scan
  register allocation, x86-64 or PTX-style emission, multiple functions with a calling convention.

## Layout

```
src/        compiler (lexer, parser, ast, sema, ir, optimizer, codegen, driver, main)
tests/      test runner
examples/   sample programs with expected results
docs/       ISA reference
```

## License

MIT
