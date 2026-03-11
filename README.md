# glsl2llvm

`glsl2llvm` is a prototype compiler frontend that lowers a subset of GLSL compute shaders to LLVM IR.

## Status

Implemented pipeline:

- Lexer
- Parser (AST)
- Semantic analysis (symbol/type checks)
- AST -> HIR lowering
- HIR -> LLVM IR codegen (LLVM `IRBuilder`, verifier)

CLI:

```bash
glsl2llvm input.comp -S -o out.ll
glsl2llvm --help
```

## Supported Subset (Current)

- Shader stage: compute (`#version 450`)
- `layout(...) in;`
- Buffer block declarations and indexed load/store
- Types: `void`, `bool`, `int`, `uint`, `float`, `vec2`, `vec3`, `vec4`
- Statements: block, `if/else`, `for`, `return`, expression statement, local/global var decl
- Expressions: assignment, `+ - * /`, `==`, `<`, `&`, call, member access, indexing, prefix `++`
- Builtins: `gl_GlobalInvocationID.{x,y,z}`, `gl_LocalInvocationID.{x,y,z}`, `barrier()`

## Known Limitations

- Not a full GLSL implementation.
- `while` is not implemented in parser.
- Multi-component swizzle like `.xy` is not implemented.
- Runtime interface is still prototype-style:
  - Entry signature is fixed to `main(i32 global_id_x, float* data)`.
  - Multiple buffer blocks are currently mapped to the same `data` pointer in codegen.
  - `global_id_y/z` are accepted but not wired to real dispatch inputs yet.
- `shared`/`barrier` currently compile in prototype form (no real memory model mapping yet).

## Build Requirements

- CMake 3.20+
- C++17 compiler
- LLVM 18 (recommended)

### Full LLVM C++ API Mode (recommended)

Required for real LLVM IR emission and full test suite (`codegen`, `e2e`, `diff`).

Example (Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake llvm-18-dev \
  libzstd-dev libcurl4-openssl-dev libxml2-dev libedit-dev libffi-dev
```

Configure and build:

```bash
cmake -S glsl2llvm -B build \
  -DGLSL2LLVM_REQUIRE_LLVM_CONFIG=ON \
  -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake
cmake --build build -j
```

### Fallback Mode (no LLVM CMake package)

The project can still build frontend pieces, but LLVM IR codegen path is disabled.
`codegen/e2e/diff` tests are skipped automatically.

```bash
cmake -S glsl2llvm -B build
cmake --build build -j
```

## Run

```bash
./build/tools/glsl2llvm/glsl2llvm examples/example.comp -S -o out.ll
```

## Test

Run all registered tests:

```bash
ctest --test-dir build --output-on-failure
```

Run only glslang differential test:

```bash
ctest --test-dir build -R diff.glslang_differential --output-on-failure
```

## Compute Shader Cases

10 compute shader test cases are provided in:

- `examples/compute-tests/`

Batch compile all 10 cases:

```bash
for f in glsl2llvm/examples/compute-tests/*.comp; do
  ./build/tools/glsl2llvm/glsl2llvm "$f" -S -o /tmp/$(basename "$f" .comp).ll || exit 1
done
```

## Docs

- `docs/roadmap.md`
- `docs/subset.md`
- `docs/ir-mapping.md`
- `docs/compat.md`
