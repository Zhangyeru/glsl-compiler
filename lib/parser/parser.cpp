#include "parser/parser.h"

#include <utility>

namespace glsl2llvm::parser {

namespace {

std::string token_description(const Token &token) {
  if (!token.lexeme.empty()) {
    return "'" + token.lexeme + "'";
  }
  return to_string(token.kind);
}

bool is_constructor_keyword(TokenKind kind) {
  return kind == TokenKind::KeywordBool || kind == TokenKind::KeywordInt ||
         kind == TokenKind::KeywordUint || kind == TokenKind::KeywordFloat ||
         kind == TokenKind::KeywordVec2 || kind == TokenKind::KeywordVec3 ||
         kind == TokenKind::KeywordVec4;
}

}  // namespace

Parser::Parser(std::string_view source, std::string source_name)
    : lexer_(source, std::move(source_name)) {
  advance();
}

ParseResult Parser::parse_translation_unit() {
  auto unit = std::make_unique<ast::TranslationUnit>(current_.location);

  while (current_.kind != TokenKind::EndOfFile) {
    if (parse_preprocessor_directive()) {
      continue;
    }

    if (parse_layout_qualifier()) {
      continue;
    }

    if (current_.kind == TokenKind::KeywordBuffer) {
      auto buffer_decl = parse_buffer_block_decl();
      if (buffer_decl != nullptr) {
        unit->global_vars.push_back(std::move(buffer_decl));
      }
      continue;
    }

    if (current_.kind == TokenKind::KeywordShared) {
      advance();
      auto shared_decl = parse_var_decl_stmt(true);
      if (shared_decl != nullptr) {
        shared_decl->is_shared = true;
        unit->global_vars.push_back(std::move(shared_decl));
      }
      continue;
    }

    if (!is_type_token(current_.kind)) {
      add_error(current_.location, "expected declaration at top level");
      advance();
      continue;
    }

    const SourceLocation decl_location = current_.location;
    std::string type_name = parse_type_name(true);
    if (type_name.empty()) {
      if (current_.kind != TokenKind::EndOfFile) {
        advance();
      }
      continue;
    }

    if (current_.kind != TokenKind::Identifier) {
      add_error(current_.location, "expected identifier after type name");
      if (current_.kind != TokenKind::EndOfFile) {
        advance();
      }
      continue;
    }

    std::string name = current_.lexeme;
    advance();

    if (match(TokenKind::LParen)) {
      auto function = parse_function_decl(decl_location, std::move(type_name), std::move(name));
      if (function != nullptr) {
        unit->functions.push_back(std::move(function));
      }
      continue;
    }

    auto global_var = parse_global_var_decl(decl_location, std::move(type_name), std::move(name));
    if (global_var != nullptr) {
      unit->global_vars.push_back(std::move(global_var));
    }
  }

  ParseResult result;
  result.unit = std::move(unit);
  result.errors = std::move(errors_);
  return result;
}

bool Parser::parse_preprocessor_directive() {
  if (current_.kind != TokenKind::Hash) {
    return false;
  }

  const SourceLocation directive_location = current_.location;
  advance();

  if (current_.kind == TokenKind::Identifier && current_.lexeme == "version") {
    advance();
    if (current_.kind == TokenKind::NumericLiteral) {
      advance();
    } else {
      add_error(current_.location, "expected numeric literal after #version");
    }
    return true;
  }

  add_error(directive_location, "unsupported preprocessor directive");

  while (current_.kind != TokenKind::EndOfFile && current_.kind != TokenKind::KeywordLayout &&
         current_.kind != TokenKind::KeywordBuffer && is_type_token(current_.kind) == false) {
    advance();
  }

  return true;
}

bool Parser::parse_layout_qualifier() {
  if (current_.kind != TokenKind::KeywordLayout) {
    return false;
  }

  advance();

  if (!expect(TokenKind::LParen, "expected '(' after layout")) {
    return true;
  }

  int depth = 1;
  while (depth > 0 && current_.kind != TokenKind::EndOfFile) {
    if (current_.kind == TokenKind::LParen) {
      ++depth;
    } else if (current_.kind == TokenKind::RParen) {
      --depth;
    }
    advance();
  }

  if (depth != 0) {
    add_error(current_.location, "unterminated layout qualifier");
    return true;
  }

  if (current_.kind == TokenKind::KeywordIn) {
    advance();
    expect(TokenKind::Semicolon, "expected ';' after layout(... ) in");
  }

  return true;
}

std::unique_ptr<ast::VarDecl> Parser::parse_buffer_block_decl() {
  const SourceLocation decl_location = current_.location;
  advance();  // buffer

  if (current_.kind != TokenKind::Identifier) {
    add_error(current_.location, "expected buffer block name after 'buffer'");
    return nullptr;
  }
  advance();  // block name

  if (!expect(TokenKind::LBrace, "expected '{' to start buffer block")) {
    return nullptr;
  }

  std::string element_type;
  std::string field_name;

  if (current_.kind != TokenKind::RBrace) {
    element_type = parse_type_name(false);

    if (current_.kind != TokenKind::Identifier) {
      add_error(current_.location, "expected buffer field name");
    } else {
      field_name = current_.lexeme;
      advance();
    }

    if (match(TokenKind::LBracket)) {
      if (current_.kind != TokenKind::RBracket) {
        (void)parse_expression();
      }
      expect(TokenKind::RBracket, "expected ']' in buffer field declaration");
    }

    expect(TokenKind::Semicolon, "expected ';' after buffer field declaration");
  }

  while (current_.kind != TokenKind::RBrace && current_.kind != TokenKind::EndOfFile) {
    advance();
  }

  expect(TokenKind::RBrace, "expected '}' to end buffer block");

  if (current_.kind != TokenKind::Identifier) {
    add_error(current_.location, "expected buffer instance name");
    return nullptr;
  }

  const std::string instance_name = current_.lexeme;
  advance();
  expect(TokenKind::Semicolon, "expected ';' after buffer instance declaration");

  auto decl = std::make_unique<ast::VarDecl>(decl_location, "__buffer", instance_name);
  decl->is_buffer_block = true;
  decl->buffer_element_type = element_type.empty() ? "float" : element_type;
  decl->buffer_field_name = field_name.empty() ? "data" : field_name;
  return decl;
}

std::unique_ptr<ast::FunctionDecl> Parser::parse_function_decl(SourceLocation location,
                                                               std::string return_type,
                                                               std::string function_name) {
  auto function = std::make_unique<ast::FunctionDecl>(location, std::move(return_type),
                                                      std::move(function_name));

  if (!match(TokenKind::RParen)) {
    while (true) {
      auto parameter = parse_parameter();
      if (parameter != nullptr) {
        function->parameters.push_back(std::move(parameter));
      }

      if (match(TokenKind::Comma)) {
        continue;
      }

      expect(TokenKind::RParen, "expected ')' after parameter list");
      break;
    }
  }

  function->body = parse_block_stmt();
  if (function->body == nullptr) {
    function->body = std::make_unique<ast::BlockStmt>(location);
  }

  return function;
}

std::unique_ptr<ast::VarDecl> Parser::parse_global_var_decl(SourceLocation location,
                                                            std::string type_name,
                                                            std::string var_name) {
  if (type_name == "void") {
    add_error(location, "global variable cannot have type void");
  }

  auto initializer = std::unique_ptr<ast::Expr>();
  if (match(TokenKind::Equal)) {
    initializer = parse_expression();
  }

  expect(TokenKind::Semicolon, "expected ';' after global variable declaration");
  return std::make_unique<ast::VarDecl>(location, std::move(type_name), std::move(var_name),
                                        std::move(initializer));
}

std::unique_ptr<ast::BlockStmt> Parser::parse_block_stmt() {
  if (current_.kind != TokenKind::LBrace) {
    add_error(current_.location, "expected '{' to start block");
    return nullptr;
  }

  const SourceLocation block_location = current_.location;
  advance();

  auto block = std::make_unique<ast::BlockStmt>(block_location);
  while (current_.kind != TokenKind::RBrace && current_.kind != TokenKind::EndOfFile) {
    auto statement = parse_statement();
    if (statement != nullptr) {
      block->statements.push_back(std::move(statement));
    }
  }

  expect(TokenKind::RBrace, "expected '}' to end block");
  return block;
}

std::unique_ptr<ast::Stmt> Parser::parse_statement() {
  switch (current_.kind) {
    case TokenKind::LBrace:
      return parse_block_stmt();
    case TokenKind::KeywordIf:
      return parse_if_stmt();
    case TokenKind::KeywordFor:
      return parse_for_stmt();
    case TokenKind::KeywordReturn:
      return parse_return_stmt();
    case TokenKind::KeywordShared: {
      advance();
      auto decl = parse_var_decl_stmt(true);
      if (decl != nullptr) {
        decl->is_shared = true;
      }
      return decl;
    }
    default:
      break;
  }

  if (begins_var_decl()) {
    return parse_var_decl_stmt(true);
  }

  return parse_expr_stmt();
}

std::unique_ptr<ast::IfStmt> Parser::parse_if_stmt() {
  const SourceLocation if_location = current_.location;
  advance();

  expect(TokenKind::LParen, "expected '(' after if");
  auto condition = parse_expression();
  expect(TokenKind::RParen, "expected ')' after if condition");

  auto then_branch = parse_statement();
  if (then_branch == nullptr) {
    then_branch = std::make_unique<ast::BlockStmt>(if_location);
  }
  std::unique_ptr<ast::Stmt> else_branch = nullptr;
  if (current_.kind == TokenKind::KeywordElse) {
    advance();
    else_branch = parse_statement();
    if (else_branch == nullptr) {
      else_branch = std::make_unique<ast::BlockStmt>(if_location);
    }
  }

  return std::make_unique<ast::IfStmt>(if_location, std::move(condition), std::move(then_branch),
                                       std::move(else_branch));
}

std::unique_ptr<ast::ForStmt> Parser::parse_for_stmt() {
  const SourceLocation for_location = current_.location;
  advance();

  auto stmt = std::make_unique<ast::ForStmt>(for_location);

  expect(TokenKind::LParen, "expected '(' after for");

  if (match(TokenKind::Semicolon)) {
    // Empty initializer.
  } else if (begins_var_decl()) {
    stmt->init_stmt = parse_var_decl_stmt(true);
  } else {
    stmt->init_expr = parse_expression();
    expect(TokenKind::Semicolon, "expected ';' after for initializer");
  }

  if (!match(TokenKind::Semicolon)) {
    stmt->condition = parse_expression();
    expect(TokenKind::Semicolon, "expected ';' after for condition");
  }

  if (!match(TokenKind::RParen)) {
    stmt->update = parse_expression();
    expect(TokenKind::RParen, "expected ')' after for update");
  }

  stmt->body = parse_statement();
  if (stmt->body == nullptr) {
    stmt->body = std::make_unique<ast::BlockStmt>(for_location);
  }

  return stmt;
}

std::unique_ptr<ast::ReturnStmt> Parser::parse_return_stmt() {
  const SourceLocation return_location = current_.location;
  advance();

  std::unique_ptr<ast::Expr> value;
  if (current_.kind != TokenKind::Semicolon) {
    value = parse_expression();
  }

  expect(TokenKind::Semicolon, "expected ';' after return statement");
  return std::make_unique<ast::ReturnStmt>(return_location, std::move(value));
}

std::unique_ptr<ast::ExprStmt> Parser::parse_expr_stmt() {
  const SourceLocation stmt_location = current_.location;
  auto expr = parse_expression();
  if (expr == nullptr) {
    add_error(stmt_location, "expected expression statement");
    synchronize_statement();
    return nullptr;
  }

  expect(TokenKind::Semicolon, "expected ';' after expression statement");
  return std::make_unique<ast::ExprStmt>(stmt_location, std::move(expr));
}

std::unique_ptr<ast::VarDecl> Parser::parse_var_decl_stmt(bool expect_semicolon) {
  const SourceLocation decl_location = current_.location;
  std::string type_name = parse_type_name(false);
  if (type_name.empty()) {
    synchronize_statement();
    return nullptr;
  }

  if (current_.kind != TokenKind::Identifier) {
    add_error(current_.location, "expected identifier after variable type");
    synchronize_statement();
    return nullptr;
  }

  std::string name = current_.lexeme;
  advance();

  bool is_array = false;
  if (match(TokenKind::LBracket)) {
    is_array = true;
    if (current_.kind != TokenKind::RBracket) {
      (void)parse_expression();
    }
    expect(TokenKind::RBracket, "expected ']' after array declarator");
  }

  auto initializer = std::unique_ptr<ast::Expr>();
  if (match(TokenKind::Equal)) {
    initializer = parse_expression();
  }

  if (expect_semicolon) {
    expect(TokenKind::Semicolon, "expected ';' after variable declaration");
  }

  if (type_name == "void") {
    add_error(decl_location, "variable cannot have type void");
  }

  auto decl = std::make_unique<ast::VarDecl>(decl_location, std::move(type_name), std::move(name),
                                             std::move(initializer));
  decl->is_array = is_array;
  return decl;
}

std::unique_ptr<ast::Expr> Parser::parse_expression() {
  return parse_assignment();
}

std::unique_ptr<ast::Expr> Parser::parse_assignment() {
  auto lhs = parse_equality();
  if (lhs == nullptr) {
    return nullptr;
  }

  if (current_.kind != TokenKind::Equal) {
    return lhs;
  }

  const SourceLocation op_location = current_.location;
  std::string op = current_.lexeme.empty() ? "=" : current_.lexeme;
  advance();

  auto rhs = parse_assignment();
  if (rhs == nullptr) {
    add_error(op_location, "expected expression after '='");
    return lhs;
  }

  return std::make_unique<ast::BinaryExpr>(op_location, std::move(op), std::move(lhs),
                                           std::move(rhs));
}

std::unique_ptr<ast::Expr> Parser::parse_equality() {
  auto expr = parse_relational();
  if (expr == nullptr) {
    return nullptr;
  }

  while (current_.kind == TokenKind::EqualEqual) {
    const SourceLocation op_location = current_.location;
    const std::string op = current_.lexeme.empty() ? std::string("==") : current_.lexeme;
    advance();

    auto rhs = parse_relational();
    if (rhs == nullptr) {
      add_error(op_location, "expected right-hand side expression");
      break;
    }

    expr = std::make_unique<ast::BinaryExpr>(op_location, op, std::move(expr), std::move(rhs));
  }

  return expr;
}

std::unique_ptr<ast::Expr> Parser::parse_relational() {
  auto expr = parse_bitwise_and();
  if (expr == nullptr) {
    return nullptr;
  }

  while (current_.kind == TokenKind::Less) {
    const SourceLocation op_location = current_.location;
    const std::string op = current_.lexeme.empty() ? std::string("<") : current_.lexeme;
    advance();

    auto rhs = parse_bitwise_and();
    if (rhs == nullptr) {
      add_error(op_location, "expected right-hand side expression");
      break;
    }

    expr = std::make_unique<ast::BinaryExpr>(op_location, op, std::move(expr), std::move(rhs));
  }

  return expr;
}

std::unique_ptr<ast::Expr> Parser::parse_bitwise_and() {
  auto expr = parse_additive();
  if (expr == nullptr) {
    return nullptr;
  }

  while (current_.kind == TokenKind::Ampersand) {
    const SourceLocation op_location = current_.location;
    const std::string op = current_.lexeme.empty() ? std::string("&") : current_.lexeme;
    advance();

    auto rhs = parse_additive();
    if (rhs == nullptr) {
      add_error(op_location, "expected right-hand side expression");
      break;
    }

    expr = std::make_unique<ast::BinaryExpr>(op_location, op, std::move(expr), std::move(rhs));
  }

  return expr;
}

std::unique_ptr<ast::Expr> Parser::parse_additive() {
  auto expr = parse_multiplicative();
  if (expr == nullptr) {
    return nullptr;
  }

  while (current_.kind == TokenKind::Plus || current_.kind == TokenKind::Minus) {
    const SourceLocation op_location = current_.location;
    const std::string op = current_.lexeme.empty()
                               ? std::string(current_.kind == TokenKind::Plus ? "+" : "-")
                               : current_.lexeme;
    advance();

    auto rhs = parse_multiplicative();
    if (rhs == nullptr) {
      add_error(op_location, "expected right-hand side expression");
      break;
    }

    expr = std::make_unique<ast::BinaryExpr>(op_location, op, std::move(expr), std::move(rhs));
  }

  return expr;
}

std::unique_ptr<ast::Expr> Parser::parse_multiplicative() {
  auto expr = parse_unary();
  if (expr == nullptr) {
    return nullptr;
  }

  while (current_.kind == TokenKind::Star || current_.kind == TokenKind::Slash) {
    const SourceLocation op_location = current_.location;
    const std::string op = current_.lexeme.empty()
                               ? std::string(current_.kind == TokenKind::Star ? "*" : "/")
                               : current_.lexeme;
    advance();

    auto rhs = parse_unary();
    if (rhs == nullptr) {
      add_error(op_location, "expected right-hand side expression");
      break;
    }

    expr = std::make_unique<ast::BinaryExpr>(op_location, op, std::move(expr), std::move(rhs));
  }

  return expr;
}

std::unique_ptr<ast::Expr> Parser::parse_unary() {
  if (current_.kind == TokenKind::PlusPlus) {
    const SourceLocation op_location = current_.location;
    advance();

    if (current_.kind != TokenKind::Identifier) {
      add_error(current_.location, "expected identifier after '++'");
      return nullptr;
    }

    const SourceLocation id_location = current_.location;
    const std::string name = current_.lexeme;
    advance();

    auto lhs_assign = std::make_unique<ast::LiteralExpr>(
        id_location, ast::LiteralExpr::ValueKind::Identifier, name);
    auto lhs_add = std::make_unique<ast::LiteralExpr>(
        id_location, ast::LiteralExpr::ValueKind::Identifier, name);
    auto one = std::make_unique<ast::LiteralExpr>(id_location, ast::LiteralExpr::ValueKind::Number,
                                                  "1");
    auto add = std::make_unique<ast::BinaryExpr>(op_location, "+", std::move(lhs_add),
                                                 std::move(one));
    return std::make_unique<ast::BinaryExpr>(op_location, "=", std::move(lhs_assign),
                                             std::move(add));
  }

  return parse_postfix();
}

std::unique_ptr<ast::Expr> Parser::parse_postfix() {
  auto expr = parse_primary();
  if (expr == nullptr) {
    return nullptr;
  }

  while (true) {
    if (current_.kind == TokenKind::LParen) {
      const SourceLocation call_location = current_.location;
      advance();

      auto call = std::make_unique<ast::CallExpr>(call_location, std::move(expr));
      if (!match(TokenKind::RParen)) {
        while (true) {
          auto arg = parse_expression();
          if (arg != nullptr) {
            call->args.push_back(std::move(arg));
          }

          if (match(TokenKind::Comma)) {
            continue;
          }

          expect(TokenKind::RParen, "expected ')' after call arguments");
          break;
        }
      }

      expr = std::move(call);
      continue;
    }

    if (current_.kind == TokenKind::Dot) {
      const SourceLocation member_location = current_.location;
      advance();

      if (current_.kind != TokenKind::Identifier) {
        add_error(current_.location, "expected identifier after '.'");
        return expr;
      }

      std::string member_name = current_.lexeme;
      advance();

      expr = std::make_unique<ast::MemberExpr>(member_location, std::move(expr),
                                               std::move(member_name));
      continue;
    }

    if (current_.kind == TokenKind::LBracket) {
      const SourceLocation index_location = current_.location;
      advance();
      auto index_expr = parse_expression();
      expect(TokenKind::RBracket, "expected ']' after index expression");
      expr = std::make_unique<ast::IndexExpr>(index_location, std::move(expr),
                                              std::move(index_expr));
      continue;
    }

    break;
  }

  return expr;
}

std::unique_ptr<ast::Expr> Parser::parse_primary() {
  if (current_.kind == TokenKind::NumericLiteral) {
    auto expr = std::make_unique<ast::LiteralExpr>(current_.location, ast::LiteralExpr::ValueKind::Number,
                                                   current_.lexeme);
    advance();
    return expr;
  }

  if (current_.kind == TokenKind::Identifier) {
    auto expr = std::make_unique<ast::LiteralExpr>(
        current_.location, ast::LiteralExpr::ValueKind::Identifier, current_.lexeme);
    advance();
    return expr;
  }

  if (is_constructor_keyword(current_.kind)) {
    auto expr = std::make_unique<ast::LiteralExpr>(
        current_.location, ast::LiteralExpr::ValueKind::Identifier, current_.lexeme);
    advance();
    return expr;
  }

  if (current_.kind == TokenKind::LParen) {
    advance();
    auto expr = parse_expression();
    expect(TokenKind::RParen, "expected ')' after parenthesized expression");
    return expr;
  }

  add_error(current_.location, "expected expression, got " + token_description(current_));

  if (current_.kind != TokenKind::Semicolon && current_.kind != TokenKind::RParen &&
      current_.kind != TokenKind::Comma && current_.kind != TokenKind::EndOfFile) {
    advance();
  }

  return nullptr;
}

std::unique_ptr<ast::VarDecl> Parser::parse_parameter() {
  const SourceLocation param_location = current_.location;
  std::string type_name = parse_type_name(false);
  if (type_name.empty()) {
    return nullptr;
  }

  if (current_.kind != TokenKind::Identifier) {
    add_error(current_.location, "expected parameter name");
    return nullptr;
  }

  std::string name = current_.lexeme;
  advance();

  auto param = std::make_unique<ast::VarDecl>(param_location, std::move(type_name), std::move(name));
  param->is_parameter = true;
  return param;
}

std::string Parser::parse_type_name(bool allow_void) {
  switch (current_.kind) {
    case TokenKind::KeywordVoid: {
      const std::string type_name = current_.lexeme.empty() ? "void" : current_.lexeme;
      if (!allow_void) {
        add_error(current_.location, "void is not allowed here");
      }
      advance();
      return type_name;
    }
    case TokenKind::KeywordBool:
    case TokenKind::KeywordInt:
    case TokenKind::KeywordUint:
    case TokenKind::KeywordFloat:
    case TokenKind::KeywordVec2:
    case TokenKind::KeywordVec3:
    case TokenKind::KeywordVec4:
    case TokenKind::Identifier: {
      const std::string type_name = current_.lexeme;
      advance();
      return type_name;
    }
    default:
      add_error(current_.location, "expected type name");
      return "";
  }
}

bool Parser::match(TokenKind kind) {
  if (current_.kind != kind) {
    return false;
  }
  advance();
  return true;
}

bool Parser::expect(TokenKind kind, std::string_view message) {
  if (current_.kind == kind) {
    advance();
    return true;
  }

  add_error(current_.location, std::string(message) + ", got " + token_description(current_));
  return false;
}

void Parser::advance() {
  current_ = lexer_.next();

  while (current_.kind == TokenKind::Error) {
    std::string message = "lexer error";
    if (!current_.error_message.empty()) {
      message += ": ";
      message += current_.error_message;
    }
    if (!current_.lexeme.empty()) {
      message += " near '";
      message += current_.lexeme;
      message += "'";
    }

    add_error(current_.location, std::move(message));
    current_ = lexer_.next();
  }
}

void Parser::add_error(SourceLocation location, std::string message) {
  errors_.push_back(ParseError{location, std::move(message)});
}

void Parser::synchronize_statement() {
  while (current_.kind != TokenKind::EndOfFile) {
    if (current_.kind == TokenKind::Semicolon) {
      advance();
      return;
    }

    if (current_.kind == TokenKind::RBrace) {
      return;
    }

    advance();
  }
}

bool Parser::is_type_token(TokenKind kind) {
  return kind == TokenKind::KeywordVoid || is_non_void_type_token(kind);
}

bool Parser::is_non_void_type_token(TokenKind kind) {
  return kind == TokenKind::KeywordBool || kind == TokenKind::KeywordInt ||
         kind == TokenKind::KeywordUint || kind == TokenKind::KeywordFloat ||
         kind == TokenKind::KeywordVec2 || kind == TokenKind::KeywordVec3 ||
         kind == TokenKind::KeywordVec4 || kind == TokenKind::Identifier;
}

bool Parser::begins_var_decl() {
  if (current_.kind == TokenKind::KeywordBool || current_.kind == TokenKind::KeywordInt ||
      current_.kind == TokenKind::KeywordUint || current_.kind == TokenKind::KeywordFloat ||
      current_.kind == TokenKind::KeywordVec2 || current_.kind == TokenKind::KeywordVec3 ||
      current_.kind == TokenKind::KeywordVec4) {
    return true;
  }

  if (current_.kind == TokenKind::Identifier) {
    const Token next = lexer_.peek();
    return next.kind == TokenKind::Identifier;
  }

  return false;
}

}  // namespace glsl2llvm::parser
