# glsl2llvm

`glsl2llvm` is a prototype frontend skeleton for compiling GLSL compute shaders to LLVM IR.

## Current Status (v0)

This initial version only provides project scaffolding and a runnable CLI:

- Reads input `.comp` file.
- Creates a placeholder LLVM `Module`.
- Writes `.ll` text output.
- Prints TODO messages for unimplemented stages.
- Supports `--help`.

Parser, semantic analysis, HIR lowering, and real codegen are not implemented yet.

## Prerequisites

- CMake 3.20+
- C++17 compiler
- LLVM runtime or development package

If LLVM CMake config files are available, the build uses them.
If they are not available, the project falls back to linking LLVM shared library and uses C API declarations.

## Build

```bash
cmake -S glsl2llvm -B build
cmake --build build -j
```

## Run

```bash
./build/tools/glsl2llvm/glsl2llvm examples/minimal.comp -S -o out.ll
```

Example help:

```bash
./build/tools/glsl2llvm/glsl2llvm --help
```

## Test

```bash
ctest --test-dir build --output-on-failure
```
