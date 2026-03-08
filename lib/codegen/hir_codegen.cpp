#include "codegen/hir_codegen.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(GLSL2LLVM_HAVE_LLVM_CONFIG)
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif

namespace glsl2llvm::codegen {

#if defined(GLSL2LLVM_HAVE_LLVM_CONFIG)
namespace {

class HIREmitter {
 public:
  HIREmitter(llvm::LLVMContext &context, llvm::Module &module)
      : context_(context), module_(module), builder_(context) {}

  bool emit(const hir::Module &hir_module) {
    declare_functions(hir_module);

    for (const auto &function : hir_module.functions) {
      emit_function_body(*function);
    }

    return errors_.empty();
  }

  const std::vector<std::string> &errors() const {
    return errors_;
  }

 private:
  struct FunctionSignature {
    sema::TypeKind return_type = sema::TypeKind::Void;
    std::vector<sema::TypeKind> parameter_types;
  };

  struct FunctionState {
    llvm::Function *function = nullptr;
    llvm::Value *global_id_x = nullptr;
    llvm::Value *data = nullptr;
    sema::TypeKind return_type = sema::TypeKind::Void;
    std::unordered_map<std::string, llvm::AllocaInst *> locals;
  };

  void add_error(std::string message) {
    errors_.push_back(std::move(message));
  }

  void declare_functions(const hir::Module &hir_module) {
    for (const auto &function : hir_module.functions) {
      std::vector<llvm::Type *> params;
      std::vector<sema::TypeKind> param_kinds;
      params.reserve(function->parameters.size() + 2);
      param_kinds.reserve(function->parameters.size());

      for (const auto &param : function->parameters) {
        params.push_back(lower_type(param.second));
        param_kinds.push_back(param.second);
      }

      params.push_back(builder_.getInt32Ty());
      params.push_back(llvm::PointerType::getUnqual(builder_.getFloatTy()));

      llvm::FunctionType *fn_type =
          llvm::FunctionType::get(lower_type(function->return_type), params, false);
      llvm::Function *fn = llvm::Function::Create(
          fn_type, llvm::Function::ExternalLinkage, function->name, module_);

      std::size_t idx = 0;
      for (auto &arg : fn->args()) {
        if (idx < function->parameters.size()) {
          arg.setName(function->parameters[idx].first);
        } else if (idx == function->parameters.size()) {
          arg.setName("global_id_x");
        } else {
          arg.setName("data");
        }
        ++idx;
      }

      function_map_[function->name] = fn;
      signatures_[function->name] = FunctionSignature{function->return_type, std::move(param_kinds)};
    }
  }

  llvm::Type *lower_type(sema::TypeKind type) const {
    switch (type) {
      case sema::TypeKind::Void:
        return llvm::Type::getVoidTy(context_);
      case sema::TypeKind::Bool:
        return llvm::Type::getInt1Ty(context_);
      case sema::TypeKind::Int:
      case sema::TypeKind::Uint:
        return llvm::Type::getInt32Ty(context_);
      case sema::TypeKind::Float:
        return llvm::Type::getFloatTy(context_);
      case sema::TypeKind::Vec2:
        return llvm::FixedVectorType::get(llvm::Type::getFloatTy(context_), 2);
      case sema::TypeKind::Vec3:
        return llvm::FixedVectorType::get(llvm::Type::getFloatTy(context_), 3);
      case sema::TypeKind::Vec4:
        return llvm::FixedVectorType::get(llvm::Type::getFloatTy(context_), 4);
      case sema::TypeKind::Error:
      case sema::TypeKind::Unresolved:
        return llvm::Type::getInt32Ty(context_);
    }

    return llvm::Type::getInt32Ty(context_);
  }

  llvm::Constant *default_value_for_type(sema::TypeKind type) const {
    llvm::Type *llvm_type = lower_type(type);
    if (llvm_type->isVoidTy()) {
      return nullptr;
    }
    return llvm::Constant::getNullValue(llvm_type);
  }

  llvm::AllocaInst *create_entry_alloca(llvm::Function *function, llvm::Type *type,
                                        const std::string &name) {
    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr, name);
  }

  void emit_function_body(const hir::Function &function) {
    const auto fn_found = function_map_.find(function.name);
    if (fn_found == function_map_.end()) {
      add_error("function not declared: " + function.name);
      return;
    }

    llvm::Function *fn = fn_found->second;
    if (!fn->empty()) {
      return;
    }

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context_, "entry", fn);
    builder_.SetInsertPoint(entry);

    FunctionState state;
    state.function = fn;
    state.return_type = function.return_type;

    auto arg_it = fn->arg_begin();
    for (const auto &param : function.parameters) {
      if (arg_it == fn->arg_end()) {
        add_error("missing function argument while materializing params: " + function.name);
        return;
      }

      llvm::Value *arg = &*arg_it++;
      llvm::AllocaInst *slot = create_entry_alloca(fn, lower_type(param.second), param.first);
      builder_.CreateStore(arg, slot);
      state.locals[param.first] = slot;
    }

    if (arg_it == fn->arg_end()) {
      add_error("missing global_id_x argument in function: " + function.name);
      return;
    }
    state.global_id_x = &*arg_it++;

    if (arg_it == fn->arg_end()) {
      add_error("missing data argument in function: " + function.name);
      return;
    }
    state.data = &*arg_it++;

    state_stack_.push_back(std::move(state));

    if (function.entry != nullptr) {
      emit_block(*function.entry);
    }

    if (builder_.GetInsertBlock() != nullptr && builder_.GetInsertBlock()->getTerminator() == nullptr) {
      if (function.return_type == sema::TypeKind::Void) {
        builder_.CreateRetVoid();
      } else {
        llvm::Constant *zero = default_value_for_type(function.return_type);
        builder_.CreateRet(zero);
      }
    }

    state_stack_.pop_back();
  }

  FunctionState &state() {
    return state_stack_.back();
  }

  const FunctionState &state() const {
    return state_stack_.back();
  }

  void emit_block(const hir::Block &block) {
    for (const auto &statement : block.statements) {
      if (builder_.GetInsertBlock() == nullptr) {
        return;
      }
      if (builder_.GetInsertBlock()->getTerminator() != nullptr) {
        return;
      }
      emit_stmt(*statement);
    }
  }

  void emit_stmt(const hir::Stmt &stmt) {
    switch (stmt.kind) {
      case hir::NodeKind::ExprStmt: {
        const auto &expr_stmt = static_cast<const hir::ExprStmt &>(stmt);
        if (expr_stmt.expression != nullptr) {
          (void)emit_expr(*expr_stmt.expression);
        }
        return;
      }
      case hir::NodeKind::VarDecl: {
        const auto &decl = static_cast<const hir::VarDecl &>(stmt);
        llvm::AllocaInst *slot = create_entry_alloca(state().function, lower_type(decl.declared_type),
                                                     decl.name);
        state().locals[decl.name] = slot;

        if (decl.initializer != nullptr) {
          llvm::Value *init = emit_expr(*decl.initializer);
          init = cast_value(init, decl.initializer->type, decl.declared_type);
          if (init != nullptr) {
            builder_.CreateStore(init, slot);
          }
        } else {
          llvm::Constant *zero = default_value_for_type(decl.declared_type);
          if (zero != nullptr) {
            builder_.CreateStore(zero, slot);
          }
        }
        return;
      }
      case hir::NodeKind::Return: {
        const auto &ret = static_cast<const hir::Return &>(stmt);
        if (ret.value == nullptr) {
          builder_.CreateRetVoid();
          return;
        }

        llvm::Value *value = emit_expr(*ret.value);
        value = cast_value(value, ret.value->type, state().return_type);
        if (value == nullptr) {
          llvm::Constant *zero = default_value_for_type(state().return_type);
          if (zero != nullptr) {
            builder_.CreateRet(zero);
          } else {
            builder_.CreateRetVoid();
          }
          return;
        }
        builder_.CreateRet(value);
        return;
      }
      case hir::NodeKind::BlockStmt: {
        const auto &block_stmt = static_cast<const hir::BlockStmt &>(stmt);
        if (block_stmt.block != nullptr) {
          emit_block(*block_stmt.block);
        }
        return;
      }
      case hir::NodeKind::If: {
        emit_if(static_cast<const hir::If &>(stmt));
        return;
      }
      case hir::NodeKind::Loop: {
        emit_loop(static_cast<const hir::Loop &>(stmt));
        return;
      }
      default:
        add_error("unsupported HIR statement node");
        return;
    }
  }

  void emit_if(const hir::If &if_stmt) {
    llvm::Function *fn = state().function;

    llvm::BasicBlock *then_bb = llvm::BasicBlock::Create(context_, "if.then", fn);
    llvm::BasicBlock *else_bb = llvm::BasicBlock::Create(context_, "if.else", fn);
    llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(context_, "if.end", fn);

    llvm::Value *cond = if_stmt.condition != nullptr ? emit_expr(*if_stmt.condition) : nullptr;
    cond = to_bool(cond, if_stmt.condition != nullptr ? if_stmt.condition->type : sema::TypeKind::Bool);

    if (cond == nullptr) {
      cond = llvm::ConstantInt::getFalse(context_);
    }

    builder_.CreateCondBr(cond, then_bb, else_bb);

    builder_.SetInsertPoint(then_bb);
    if (if_stmt.then_block != nullptr) {
      emit_block(*if_stmt.then_block);
    }
    if (builder_.GetInsertBlock() != nullptr && builder_.GetInsertBlock()->getTerminator() == nullptr) {
      builder_.CreateBr(merge_bb);
    }

    builder_.SetInsertPoint(else_bb);
    if (if_stmt.else_block != nullptr) {
      emit_block(*if_stmt.else_block);
    }
    if (builder_.GetInsertBlock() != nullptr && builder_.GetInsertBlock()->getTerminator() == nullptr) {
      builder_.CreateBr(merge_bb);
    }

    builder_.SetInsertPoint(merge_bb);
  }

  void emit_loop(const hir::Loop &loop) {
    llvm::Function *fn = state().function;

    if (loop.init != nullptr) {
      emit_stmt(*loop.init);
    }

    llvm::BasicBlock *cond_bb = llvm::BasicBlock::Create(context_, "loop.cond", fn);
    llvm::BasicBlock *body_bb = llvm::BasicBlock::Create(context_, "loop.body", fn);
    llvm::BasicBlock *update_bb = llvm::BasicBlock::Create(context_, "loop.update", fn);
    llvm::BasicBlock *exit_bb = llvm::BasicBlock::Create(context_, "loop.end", fn);

    if (builder_.GetInsertBlock() != nullptr && builder_.GetInsertBlock()->getTerminator() == nullptr) {
      builder_.CreateBr(cond_bb);
    }

    builder_.SetInsertPoint(cond_bb);
    llvm::Value *cond = nullptr;
    if (loop.condition != nullptr) {
      cond = emit_expr(*loop.condition);
      cond = to_bool(cond, loop.condition->type);
    }
    if (cond == nullptr) {
      cond = llvm::ConstantInt::getTrue(context_);
    }
    builder_.CreateCondBr(cond, body_bb, exit_bb);

    builder_.SetInsertPoint(body_bb);
    if (loop.body != nullptr) {
      emit_block(*loop.body);
    }
    if (builder_.GetInsertBlock() != nullptr && builder_.GetInsertBlock()->getTerminator() == nullptr) {
      builder_.CreateBr(update_bb);
    }

    builder_.SetInsertPoint(update_bb);
    if (loop.update != nullptr) {
      (void)emit_expr(*loop.update);
    }
    if (builder_.GetInsertBlock() != nullptr && builder_.GetInsertBlock()->getTerminator() == nullptr) {
      builder_.CreateBr(cond_bb);
    }

    builder_.SetInsertPoint(exit_bb);
  }

  llvm::Value *emit_expr(const hir::Expr &expr) {
    switch (expr.kind) {
      case hir::NodeKind::Literal:
        return emit_literal(static_cast<const hir::Literal &>(expr));
      case hir::NodeKind::Variable:
        return emit_variable(static_cast<const hir::Variable &>(expr));
      case hir::NodeKind::Binary:
        return emit_binary(static_cast<const hir::Binary &>(expr));
      case hir::NodeKind::Call:
        return emit_call(static_cast<const hir::Call &>(expr));
      case hir::NodeKind::BuiltinLoad:
        return emit_builtin_load(static_cast<const hir::BuiltinLoad &>(expr));
      case hir::NodeKind::BufferLoad:
        return emit_buffer_load(static_cast<const hir::BufferLoad &>(expr));
      case hir::NodeKind::BufferStore:
        return emit_buffer_store(static_cast<const hir::BufferStore &>(expr));
      case hir::NodeKind::Cast:
        return emit_cast(static_cast<const hir::Cast &>(expr));
      default:
        add_error("unsupported HIR expression node");
        return nullptr;
    }
  }

  llvm::Value *emit_literal(const hir::Literal &literal) {
    switch (literal.type) {
      case sema::TypeKind::Float:
        return llvm::ConstantFP::get(builder_.getFloatTy(), llvm::APFloat(static_cast<float>(std::stod(literal.value))));
      case sema::TypeKind::Int:
      case sema::TypeKind::Uint:
        return llvm::ConstantInt::get(builder_.getInt32Ty(), static_cast<uint64_t>(std::stoll(literal.value)), false);
      case sema::TypeKind::Bool: {
        const bool truthy = literal.value == "true" || literal.value == "1";
        return llvm::ConstantInt::get(builder_.getInt1Ty(), truthy ? 1 : 0, false);
      }
      default:
        if (llvm::Type *ty = lower_type(literal.type); !ty->isVoidTy()) {
          return llvm::Constant::getNullValue(ty);
        }
        return nullptr;
    }
  }

  llvm::Value *emit_variable(const hir::Variable &variable) {
    const auto found = state().locals.find(variable.name);
    if (found == state().locals.end()) {
      add_error("unknown local variable in codegen: " + variable.name);
      return llvm::Constant::getNullValue(lower_type(variable.type));
    }

    return builder_.CreateLoad(lower_type(variable.type), found->second, variable.name + ".load");
  }

  llvm::Value *emit_binary(const hir::Binary &binary) {
    if (binary.op == "=") {
      return emit_assignment(binary);
    }

    llvm::Value *lhs = binary.lhs != nullptr ? emit_expr(*binary.lhs) : nullptr;
    llvm::Value *rhs = binary.rhs != nullptr ? emit_expr(*binary.rhs) : nullptr;

    if (lhs == nullptr || rhs == nullptr) {
      add_error("binary expression missing operand");
      return nullptr;
    }

    if (sema::is_vector_type(binary.type) || binary.type == sema::TypeKind::Float) {
      if (binary.op == "+") {
        return builder_.CreateFAdd(lhs, rhs, "fadd.tmp");
      }
      if (binary.op == "-") {
        return builder_.CreateFSub(lhs, rhs, "fsub.tmp");
      }
      if (binary.op == "*") {
        return builder_.CreateFMul(lhs, rhs, "fmul.tmp");
      }
      if (binary.op == "/") {
        return builder_.CreateFDiv(lhs, rhs, "fdiv.tmp");
      }
    } else {
      if (binary.op == "+") {
        return builder_.CreateAdd(lhs, rhs, "add.tmp");
      }
      if (binary.op == "-") {
        return builder_.CreateSub(lhs, rhs, "sub.tmp");
      }
      if (binary.op == "*") {
        return builder_.CreateMul(lhs, rhs, "mul.tmp");
      }
      if (binary.op == "/") {
        if (binary.type == sema::TypeKind::Uint) {
          return builder_.CreateUDiv(lhs, rhs, "udiv.tmp");
        }
        return builder_.CreateSDiv(lhs, rhs, "sdiv.tmp");
      }
    }

    add_error("unsupported binary operator: " + binary.op);
    return nullptr;
  }

  llvm::Value *emit_assignment(const hir::Binary &binary) {
    if (binary.lhs == nullptr || binary.rhs == nullptr) {
      add_error("assignment requires both lhs and rhs");
      return nullptr;
    }

    if (binary.lhs->kind == hir::NodeKind::Variable) {
      const auto *lhs_var = static_cast<const hir::Variable *>(binary.lhs.get());
      const auto found = state().locals.find(lhs_var->name);
      if (found == state().locals.end()) {
        add_error("assignment to unknown variable: " + lhs_var->name);
        return nullptr;
      }

      llvm::Value *rhs = emit_expr(*binary.rhs);
      rhs = cast_value(rhs, binary.rhs->type, lhs_var->type);
      if (rhs == nullptr) {
        return nullptr;
      }

      builder_.CreateStore(rhs, found->second);
      return rhs;
    }

    if (binary.lhs->kind == hir::NodeKind::BufferLoad) {
      const auto *lhs_buffer = static_cast<const hir::BufferLoad *>(binary.lhs.get());
      llvm::Value *index = lhs_buffer->index != nullptr ? emit_expr(*lhs_buffer->index) : nullptr;
      index = cast_value(index, lhs_buffer->index != nullptr ? lhs_buffer->index->type : sema::TypeKind::Int,
                         sema::TypeKind::Int);
      if (index == nullptr) {
        index = llvm::ConstantInt::get(builder_.getInt32Ty(), 0);
      }

      llvm::Value *idx64 = builder_.CreateSExt(index, builder_.getInt64Ty());
      llvm::Value *ptr =
          builder_.CreateGEP(builder_.getFloatTy(), state().data, idx64, lhs_buffer->buffer_name + ".ptr");

      llvm::Value *rhs = emit_expr(*binary.rhs);
      rhs = cast_value(rhs, binary.rhs->type, sema::TypeKind::Float);
      if (rhs == nullptr) {
        return nullptr;
      }
      builder_.CreateStore(rhs, ptr);
      return rhs;
    }

    add_error("assignment lhs is not storable");
    return nullptr;
  }

  llvm::Value *emit_call(const hir::Call &call) {
    if (call.callee.rfind("__ctor.vec", 0) == 0) {
      return emit_vector_constructor(call);
    }

    const auto fn_found = function_map_.find(call.callee);
    if (fn_found == function_map_.end()) {
      add_error("call to unknown function: " + call.callee);
      if (llvm::Type *ty = lower_type(call.type); !ty->isVoidTy()) {
        return llvm::Constant::getNullValue(ty);
      }
      return nullptr;
    }

    llvm::Function *callee = fn_found->second;
    const auto sig_found = signatures_.find(call.callee);
    if (sig_found == signatures_.end()) {
      add_error("missing signature for function: " + call.callee);
      return nullptr;
    }

    const FunctionSignature &sig = sig_found->second;
    if (call.args.size() != sig.parameter_types.size()) {
      add_error("argument count mismatch when emitting call: " + call.callee);
      return nullptr;
    }

    std::vector<llvm::Value *> args;
    args.reserve(call.args.size() + 2);

    for (std::size_t i = 0; i < call.args.size(); ++i) {
      llvm::Value *arg = emit_expr(*call.args[i]);
      arg = cast_value(arg, call.args[i]->type, sig.parameter_types[i]);
      args.push_back(arg);
    }

    args.push_back(state().global_id_x);
    args.push_back(state().data);

    llvm::CallInst *inst = builder_.CreateCall(callee, args);
    if (call.type == sema::TypeKind::Void) {
      return nullptr;
    }
    return inst;
  }

  llvm::Value *emit_vector_constructor(const hir::Call &call) {
    if (call.type != sema::TypeKind::Vec2 && call.type != sema::TypeKind::Vec3 &&
        call.type != sema::TypeKind::Vec4) {
      add_error("invalid vector constructor return type");
      return nullptr;
    }

    llvm::Type *vec_type = lower_type(call.type);
    llvm::Value *vec = llvm::UndefValue::get(vec_type);

    const int lane_count = call.type == sema::TypeKind::Vec2 ? 2 : (call.type == sema::TypeKind::Vec3 ? 3 : 4);
    for (int i = 0; i < lane_count; ++i) {
      llvm::Value *scalar = nullptr;
      if (i < static_cast<int>(call.args.size())) {
        scalar = emit_expr(*call.args[static_cast<std::size_t>(i)]);
        scalar = cast_value(scalar, call.args[static_cast<std::size_t>(i)]->type, sema::TypeKind::Float);
      }
      if (scalar == nullptr) {
        scalar = llvm::ConstantFP::get(builder_.getFloatTy(), 0.0);
      }

      vec = builder_.CreateInsertElement(vec, scalar, builder_.getInt32(i), "vec.ins");
    }

    return vec;
  }

  llvm::Value *emit_builtin_load(const hir::BuiltinLoad &load) {
    if (load.builtin_name != "gl_GlobalInvocationID") {
      add_error("unsupported builtin load: " + load.builtin_name);
      return nullptr;
    }

    if (load.component.empty() || load.component == "x") {
      if (load.type == sema::TypeKind::Float) {
        return builder_.CreateUIToFP(state().global_id_x, builder_.getFloatTy(), "gid.x.f");
      }
      if (load.type == sema::TypeKind::Int || load.type == sema::TypeKind::Uint) {
        return state().global_id_x;
      }
      if (load.type == sema::TypeKind::Bool) {
        return builder_.CreateICmpNE(state().global_id_x, builder_.getInt32(0), "gid.x.b");
      }
      if (load.type == sema::TypeKind::Vec2 || load.type == sema::TypeKind::Vec3 ||
          load.type == sema::TypeKind::Vec4) {
        llvm::Value *x = builder_.CreateUIToFP(state().global_id_x, builder_.getFloatTy(), "gid.x.f");
        llvm::Value *vec = llvm::UndefValue::get(lower_type(load.type));
        const int lanes = load.type == sema::TypeKind::Vec2 ? 2 : (load.type == sema::TypeKind::Vec3 ? 3 : 4);
        for (int i = 0; i < lanes; ++i) {
          llvm::Value *elem = i == 0 ? x : llvm::ConstantFP::get(builder_.getFloatTy(), 0.0);
          vec = builder_.CreateInsertElement(vec, elem, builder_.getInt32(i), "gid.vec");
        }
        return vec;
      }
    }

    add_error("unsupported gl_GlobalInvocationID component: " + load.component);
    return nullptr;
  }

  llvm::Value *emit_buffer_load(const hir::BufferLoad &load) {
    llvm::Value *index = load.index != nullptr ? emit_expr(*load.index) : nullptr;
    index = cast_value(index, load.index != nullptr ? load.index->type : sema::TypeKind::Int,
                       sema::TypeKind::Int);
    if (index == nullptr) {
      index = builder_.getInt32(0);
    }

    llvm::Value *idx64 = builder_.CreateSExt(index, builder_.getInt64Ty());
    llvm::Value *ptr = builder_.CreateGEP(builder_.getFloatTy(), state().data, idx64, "data.gep");
    llvm::Value *loaded = builder_.CreateLoad(builder_.getFloatTy(), ptr, "data.load");

    return cast_value(loaded, sema::TypeKind::Float, load.type);
  }

  llvm::Value *emit_buffer_store(const hir::BufferStore &store) {
    llvm::Value *index = store.index != nullptr ? emit_expr(*store.index) : nullptr;
    index = cast_value(index, store.index != nullptr ? store.index->type : sema::TypeKind::Int,
                       sema::TypeKind::Int);
    if (index == nullptr) {
      index = builder_.getInt32(0);
    }

    llvm::Value *value = store.value != nullptr ? emit_expr(*store.value) : nullptr;
    value = cast_value(value, store.value != nullptr ? store.value->type : sema::TypeKind::Float,
                       sema::TypeKind::Float);
    if (value == nullptr) {
      value = llvm::ConstantFP::get(builder_.getFloatTy(), 0.0);
    }

    llvm::Value *idx64 = builder_.CreateSExt(index, builder_.getInt64Ty());
    llvm::Value *ptr = builder_.CreateGEP(builder_.getFloatTy(), state().data, idx64, "data.gep");
    builder_.CreateStore(value, ptr);
    return nullptr;
  }

  llvm::Value *emit_cast(const hir::Cast &cast) {
    llvm::Value *value = cast.value != nullptr ? emit_expr(*cast.value) : nullptr;
    return cast_value(value, cast.from_type, cast.to_type);
  }

  llvm::Value *to_bool(llvm::Value *value, sema::TypeKind from) {
    return cast_value(value, from, sema::TypeKind::Bool);
  }

  llvm::Value *cast_value(llvm::Value *value, sema::TypeKind from, sema::TypeKind to) {
    if (value == nullptr) {
      return nullptr;
    }

    if (from == to) {
      return value;
    }

    if (to == sema::TypeKind::Bool) {
      if (from == sema::TypeKind::Bool) {
        return value;
      }
      if (from == sema::TypeKind::Int || from == sema::TypeKind::Uint) {
        return builder_.CreateICmpNE(value, builder_.getInt32(0), "cast.i32.bool");
      }
      if (from == sema::TypeKind::Float) {
        return builder_.CreateFCmpONE(value, llvm::ConstantFP::get(builder_.getFloatTy(), 0.0),
                                      "cast.f32.bool");
      }
      return value;
    }

    if (from == sema::TypeKind::Bool && (to == sema::TypeKind::Int || to == sema::TypeKind::Uint)) {
      return builder_.CreateZExt(value, builder_.getInt32Ty(), "cast.bool.i32");
    }
    if (from == sema::TypeKind::Bool && to == sema::TypeKind::Float) {
      return builder_.CreateUIToFP(value, builder_.getFloatTy(), "cast.bool.f32");
    }

    if ((from == sema::TypeKind::Int || from == sema::TypeKind::Uint) && to == sema::TypeKind::Float) {
      if (from == sema::TypeKind::Uint) {
        return builder_.CreateUIToFP(value, builder_.getFloatTy(), "cast.u32.f32");
      }
      return builder_.CreateSIToFP(value, builder_.getFloatTy(), "cast.i32.f32");
    }

    if (from == sema::TypeKind::Float && (to == sema::TypeKind::Int || to == sema::TypeKind::Uint)) {
      if (to == sema::TypeKind::Uint) {
        return builder_.CreateFPToUI(value, builder_.getInt32Ty(), "cast.f32.u32");
      }
      return builder_.CreateFPToSI(value, builder_.getInt32Ty(), "cast.f32.i32");
    }

    if ((from == sema::TypeKind::Int && to == sema::TypeKind::Uint) ||
        (from == sema::TypeKind::Uint && to == sema::TypeKind::Int)) {
      return value;
    }

    if (value->getType() == lower_type(to)) {
      return value;
    }

    return value;
  }

  llvm::LLVMContext &context_;
  llvm::Module &module_;
  llvm::IRBuilder<> builder_;

  std::unordered_map<std::string, llvm::Function *> function_map_;
  std::unordered_map<std::string, FunctionSignature> signatures_;
  std::vector<FunctionState> state_stack_;
  std::vector<std::string> errors_;
};

}  // namespace

LLVMCodegenResult HIRCodeGenerator::generate(const hir::Module &hir_module, std::string module_name) {
  LLVMCodegenResult result;
  result.context = decltype(result.context)(new llvm::LLVMContext());
  result.module = decltype(result.module)(new llvm::Module(module_name, *result.context));

  HIREmitter emitter(*result.context, *result.module);
  (void)emitter.emit(hir_module);

  result.errors = emitter.errors();

  std::string verify_error;
  llvm::raw_string_ostream verify_stream(verify_error);
  if (llvm::verifyModule(*result.module, &verify_stream)) {
    result.errors.push_back("LLVM verifier failed: " + verify_stream.str());
  }

  if (!result.errors.empty()) {
    result.module.reset();
    result.context.reset();
  }

  return result;
}

std::string module_to_string(const llvm::Module &module) {
  std::string text;
  llvm::raw_string_ostream os(text);
  module.print(os, nullptr);
  os.flush();
  return text;
}

#else

LLVMCodegenResult HIRCodeGenerator::generate(const hir::Module &, std::string) {
  LLVMCodegenResult result;
  result.errors.push_back("HIRCodeGenerator requires LLVM C++ API (LLVM CONFIG mode).");
  return result;
}

std::string module_to_string(const llvm::Module &) {
  return "";
}

#endif

}  // namespace glsl2llvm::codegen
