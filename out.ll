; ModuleID = '/home/anfield/project/glsl-compiler/glsl2llvm/examples/example.comp'
source_filename = "/home/anfield/project/glsl-compiler/glsl2llvm/examples/example.comp"

define void @main(i32 %global_id_x, ptr %data) {
entry:
  %i = alloca i32, align 4
  store i32 %global_id_x, ptr %i, align 4
  %i.load = load i32, ptr %i, align 4
  %0 = sext i32 %i.load to i64
  %buf.ptr = getelementptr float, ptr %data, i64 %0
  %i.load1 = load i32, ptr %i, align 4
  %1 = sext i32 %i.load1 to i64
  %data.gep = getelementptr float, ptr %data, i64 %1
  %data.load = load float, ptr %data.gep, align 4
  %fmul.tmp = fmul float %data.load, 2.000000e+00
  store float %fmul.tmp, ptr %buf.ptr, align 4
  ret void
}
