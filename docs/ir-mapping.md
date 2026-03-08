# GLSL to LLVM IR Mapping (Draft)

This draft captures high-level mapping intent from frontend IR to LLVM IR.

## Core Ideas

- Each shader entry point maps to an LLVM function.
- GLSL scalar/vector math lowers to LLVM integer/float ops.
- Structured control flow lowers to LLVM basic blocks and branches.
- Frontend symbols map to SSA values where possible.

## Preliminary Mapping Table

| GLSL/HIR construct | LLVM IR concept |
| --- | --- |
| `void main()` | `define void @main()` |
| integer literal | `i32` constant |
| float literal | `float` constant |
| add/sub/mul/div | `add/sub/mul/sdiv`, `fadd/fsub/fmul/fdiv` |
| comparison | `icmp` / `fcmp` |
| `if` | conditional `br` + merge block |
| loops | loop header/body/latch blocks + `br` |
| local variable | `alloca` + `load/store` (initial version) |

## Notes

- Initial codegen may use `alloca`-heavy form for simplicity.
- Canonical SSA promotion can be added later via LLVM passes.
- Resource model and address spaces need a dedicated design pass.
