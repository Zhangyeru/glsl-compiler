# glsl2llvm vs glslangValidator Compatibility

Last validated on: 2026-03-08

Differential test suite location:
- `tests/diff`
- Manifest: `tests/diff/cases.json`
- Generated run report (from CTest run): `build/tests/diff/compatibility-report.md`

## Summary

- Total differential cases: 11
- Case checks passed: 11
- Behavior/semantic differences: 6

## Case Coverage

| Case | glsl2llvm | glslangValidator | Notes |
| --- | --- | --- | --- |
| `ok.buffer_scale` | pass | pass | Minimal buffer read/modify/write kernel |
| `ok.layout_xyz` | pass | pass | `layout(local_size_x/y/z)` accepted |
| `match.undefined_symbol` | fail | fail | Undefined identifier diagnostics align |
| `match.duplicate_definition` | fail | fail | Duplicate local definition diagnostics align |
| `match.type_mismatch` | fail | fail | Scalar type mismatch diagnostics align |
| `diff.while_loop` | fail | pass | `while` not supported by parser |
| `diff.if_else` | fail | pass | `else` branch not supported by parser |
| `diff.vec_ctor_swizzle` | fail | fail | glsl2llvm misses constructor support in sema; glslang reports conversion issue |
| `diff.multi_buffer_field` | fail | pass | Only `buffer { float data[]; }` shape is supported |
| `diff.less_than_operator` | fail | pass | `<` comparison token/op not implemented |
| `diff.global_id_y` | fail | pass | codegen currently only supports `gl_GlobalInvocationID.x` |

## Unsupported Features (Current)

- `while` statement parsing
- `if (...) ... else ...` parsing
- Comparison operators like `<`
- Vector constructors in semantic analysis (`vec2(...)`, etc.)
- Multi-component swizzle/member behavior (`.xy`)
- Buffer block fields other than canonical `.data`
- Builtin component codegen beyond `gl_GlobalInvocationID.x`

## Semantic Differences

- Some failures happen at different stages:
  - glsl2llvm often fails in parser/sema for unsupported syntax.
  - glslangValidator accepts full GLSL syntax and fails later only on real GLSL semantic errors.
- In `diff.vec_ctor_swizzle`, both compilers fail, but diagnostic semantics differ:
  - glsl2llvm: undefined constructor symbol / unsupported member model.
  - glslangValidator: type conversion diagnostic.

## How To Re-run

```bash
cmake -S glsl2llvm -B build
cmake --build build -j
ctest --test-dir build -R diff.glslang_differential --output-on-failure
```
