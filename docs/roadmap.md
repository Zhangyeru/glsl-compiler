# Roadmap

## Phase 0: Skeleton (current)

- Set up CMake project structure.
- Integrate LLVM dependency.
- Provide runnable CLI entry point.
- Emit placeholder LLVM module and TODO messages.
- Add minimal tests and examples.

## Phase 1: Frontend Core

- Implement lexer for GLSL subset.
- Implement parser for compute-shader subset.
- Build AST node definitions and diagnostics.

## Phase 2: Semantics + HIR

- Type checking and symbol resolution.
- Builtin handling for compute stage.
- Lower AST to HIR.

## Phase 3: LLVM IR Codegen

- Map HIR ops to LLVM IR.
- Handle control flow and SSA values.
- Support resource/buffer access model.

## Phase 4: Validation and Tooling

- Add golden tests for IR output.
- Add error recovery and source locations.
- Extend CLI flags (`-emit-llvm`, optimization levels, debug output).
