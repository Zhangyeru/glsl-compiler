#pragma once

#include "ast/ast.h"
#include "parser/lexer.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace glsl2llvm::parser {

struct ParseError {
  SourceLocation location;
  std::string message;
};

struct ParseResult {
  std::unique_ptr<ast::TranslationUnit> unit;
  std::vector<ParseError> errors;

  [[nodiscard]] bool ok() const { return errors.empty(); }
};

class Parser {
 public:
  Parser(std::string_view source, std::string source_name = "<memory>");

  ParseResult parse_translation_unit();

 private:
  bool parse_preprocessor_directive();
  bool parse_layout_qualifier();
  std::unique_ptr<ast::VarDecl> parse_buffer_block_decl();

  std::unique_ptr<ast::FunctionDecl> parse_function_decl(SourceLocation location, std::string return_type,
                                                         std::string function_name);
  std::unique_ptr<ast::VarDecl> parse_global_var_decl(SourceLocation location, std::string type_name,
                                                      std::string var_name);

  std::unique_ptr<ast::BlockStmt> parse_block_stmt();
  std::unique_ptr<ast::Stmt> parse_statement();
  std::unique_ptr<ast::IfStmt> parse_if_stmt();
  std::unique_ptr<ast::ForStmt> parse_for_stmt();
  std::unique_ptr<ast::ReturnStmt> parse_return_stmt();
  std::unique_ptr<ast::ExprStmt> parse_expr_stmt();
  std::unique_ptr<ast::VarDecl> parse_var_decl_stmt(bool expect_semicolon = true);

  std::unique_ptr<ast::Expr> parse_expression();
  std::unique_ptr<ast::Expr> parse_assignment();
  std::unique_ptr<ast::Expr> parse_additive();
  std::unique_ptr<ast::Expr> parse_multiplicative();
  std::unique_ptr<ast::Expr> parse_postfix();
  std::unique_ptr<ast::Expr> parse_primary();

  std::unique_ptr<ast::VarDecl> parse_parameter();
  std::string parse_type_name(bool allow_void);

  bool match(TokenKind kind);
  bool expect(TokenKind kind, std::string_view message);
  void advance();
  void add_error(SourceLocation location, std::string message);
  void synchronize_statement();

  static bool is_type_token(TokenKind kind);
  static bool is_non_void_type_token(TokenKind kind);
  bool begins_var_decl();

  Lexer lexer_;
  Token current_;
  std::vector<ParseError> errors_;
};

}  // namespace glsl2llvm::parser
