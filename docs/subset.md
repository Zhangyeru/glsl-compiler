# Initial GLSL Subset (Planned)

This document defines the first parser/codegen target subset for compute shaders.

## In Scope

- Shader stage: compute only.
- `#version 450`.
- `layout(local_size_x=..., local_size_y=..., local_size_z=...) in;`
- Function declarations with `void main()`.
- Scalar types: `bool`, `int`, `uint`, `float`.
- Basic expressions: literals, unary/binary arithmetic, comparisons.
- Statements: block, `if`, `for`, `while`, `return`.

## Out of Scope (v1)

- Graphics stages (`vertex`, `fragment`, etc.).
- Pointers and advanced memory qualifiers.
- Complex preprocessor features.
- Full stdlib/builtin surface.
- Full layout and binding model.

## Acceptance Criteria (for first real frontend milestone)

- Parse valid subset programs.
- Produce clear diagnostics for unsupported syntax.
- Generate stable HIR for all accepted syntax.
