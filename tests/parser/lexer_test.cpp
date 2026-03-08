#include "parser/lexer.h"
#include "parser/token.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using glsl2llvm::parser::Lexer;
using glsl2llvm::parser::Token;
using glsl2llvm::parser::TokenKind;

bool fail(const std::string &message) {
  std::cerr << message << "\n";
  return false;
}

bool expect_kind(const Token &token, TokenKind expected, const std::string &label) {
  if (token.kind != expected) {
    return fail(label + ": expected " + glsl2llvm::parser::to_string(expected) + ", got " +
                glsl2llvm::parser::to_string(token.kind));
  }
  return true;
}

bool expect_lexeme(const Token &token, const std::string &expected, const std::string &label) {
  if (token.lexeme != expected) {
    return fail(label + ": expected lexeme '" + expected + "', got '" + token.lexeme + "'");
  }
  return true;
}

bool expect_location(const Token &token, int line, int column, const std::string &label) {
  if (token.location.line != line || token.location.column != column) {
    return fail(label + ": expected location " + std::to_string(line) + ":" +
                std::to_string(column) + ", got " + std::to_string(token.location.line) + ":" +
                std::to_string(token.location.column));
  }
  return true;
}

std::vector<Token> lex_all(const std::string &source) {
  Lexer lexer(source);
  std::vector<Token> tokens;
  for (;;) {
    Token token = lexer.next();
    tokens.push_back(token);
    if (token.kind == TokenKind::EndOfFile) {
      break;
    }
  }
  return tokens;
}

bool case_keywords() {
  const std::string source = "void bool int uint float vec2 vec3 vec4 if for return";
  const std::vector<Token> tokens = lex_all(source);

  const std::vector<TokenKind> expected = {
      TokenKind::KeywordVoid,   TokenKind::KeywordBool, TokenKind::KeywordInt,
      TokenKind::KeywordUint,   TokenKind::KeywordFloat, TokenKind::KeywordVec2,
      TokenKind::KeywordVec3,   TokenKind::KeywordVec4, TokenKind::KeywordIf,
      TokenKind::KeywordFor,    TokenKind::KeywordReturn, TokenKind::EndOfFile,
  };

  if (tokens.size() != expected.size()) {
    return fail("keywords: unexpected token count");
  }

  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (!expect_kind(tokens[i], expected[i], "keywords token " + std::to_string(i))) {
      return false;
    }
  }

  return true;
}

bool case_symbols() {
  const std::string source = "{ } ( ) ; , . = + - * /";
  const std::vector<Token> tokens = lex_all(source);

  const std::vector<TokenKind> expected = {
      TokenKind::LBrace,    TokenKind::RBrace, TokenKind::LParen, TokenKind::RParen,
      TokenKind::Semicolon, TokenKind::Comma,  TokenKind::Dot,    TokenKind::Equal,
      TokenKind::Plus,      TokenKind::Minus,  TokenKind::Star,   TokenKind::Slash,
      TokenKind::EndOfFile,
  };

  if (tokens.size() != expected.size()) {
    return fail("symbols: unexpected token count");
  }

  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (!expect_kind(tokens[i], expected[i], "symbols token " + std::to_string(i))) {
      return false;
    }
  }

  return true;
}

bool case_identifiers() {
  const std::string source = "_a foo bar123 vec5 returnValue";
  const std::vector<Token> tokens = lex_all(source);

  const std::vector<std::string> expected_lexemes = {
      "_a",
      "foo",
      "bar123",
      "vec5",
      "returnValue",
  };

  if (tokens.size() != expected_lexemes.size() + 1) {
    return fail("identifiers: unexpected token count");
  }

  for (std::size_t i = 0; i < expected_lexemes.size(); ++i) {
    if (!expect_kind(tokens[i], TokenKind::Identifier, "identifiers token " + std::to_string(i))) {
      return false;
    }
    if (!expect_lexeme(tokens[i], expected_lexemes[i], "identifiers token " + std::to_string(i))) {
      return false;
    }
  }

  return expect_kind(tokens.back(), TokenKind::EndOfFile, "identifiers eof");
}

bool case_numeric_literals() {
  const std::string source = "0 42 123 3.14 10.25";
  const std::vector<Token> tokens = lex_all(source);

  const std::vector<std::string> expected_lexemes = {"0", "42", "123", "3.14", "10.25"};

  if (tokens.size() != expected_lexemes.size() + 1) {
    return fail("numeric_literals: unexpected token count");
  }

  for (std::size_t i = 0; i < expected_lexemes.size(); ++i) {
    if (!expect_kind(tokens[i], TokenKind::NumericLiteral,
                     "numeric_literals token " + std::to_string(i))) {
      return false;
    }
    if (!expect_lexeme(tokens[i], expected_lexemes[i],
                       "numeric_literals token " + std::to_string(i))) {
      return false;
    }
  }

  return expect_kind(tokens.back(), TokenKind::EndOfFile, "numeric_literals eof");
}

bool case_mixed_snippet() {
  const std::string source = "void main(){return;}";
  const std::vector<Token> tokens = lex_all(source);

  const std::vector<TokenKind> expected = {
      TokenKind::KeywordVoid,   TokenKind::Identifier, TokenKind::LParen,
      TokenKind::RParen,        TokenKind::LBrace,     TokenKind::KeywordReturn,
      TokenKind::Semicolon,     TokenKind::RBrace,     TokenKind::EndOfFile,
  };

  if (tokens.size() != expected.size()) {
    return fail("mixed_snippet: unexpected token count");
  }

  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (!expect_kind(tokens[i], expected[i], "mixed_snippet token " + std::to_string(i))) {
      return false;
    }
  }

  return true;
}

bool case_source_location() {
  Lexer lexer("void\n  x");
  const Token t0 = lexer.next();
  const Token t1 = lexer.next();
  const Token t2 = lexer.next();

  if (!expect_kind(t0, TokenKind::KeywordVoid, "source_location t0")) {
    return false;
  }
  if (!expect_location(t0, 1, 1, "source_location t0")) {
    return false;
  }

  if (!expect_kind(t1, TokenKind::Identifier, "source_location t1")) {
    return false;
  }
  if (!expect_location(t1, 2, 3, "source_location t1")) {
    return false;
  }

  return expect_kind(t2, TokenKind::EndOfFile, "source_location eof");
}

bool case_peek() {
  Lexer lexer("int value");

  const Token p0 = lexer.peek();
  const Token p1 = lexer.peek();
  const Token n0 = lexer.next();
  const Token n1 = lexer.next();

  if (!expect_kind(p0, TokenKind::KeywordInt, "peek p0")) {
    return false;
  }
  if (!expect_kind(p1, TokenKind::KeywordInt, "peek p1")) {
    return false;
  }
  if (!expect_kind(n0, TokenKind::KeywordInt, "peek n0")) {
    return false;
  }
  if (!expect_kind(n1, TokenKind::Identifier, "peek n1")) {
    return false;
  }

  return true;
}

bool case_error_token() {
  Lexer lexer("@");

  const Token t0 = lexer.next();
  const Token t1 = lexer.next();

  if (!expect_kind(t0, TokenKind::Error, "error_token t0")) {
    return false;
  }
  if (!expect_lexeme(t0, "@", "error_token t0")) {
    return false;
  }
  if (!expect_location(t0, 1, 1, "error_token t0")) {
    return false;
  }

  return expect_kind(t1, TokenKind::EndOfFile, "error_token eof");
}

bool case_slash_minus() {
  const std::vector<Token> tokens = lex_all("/ - /");
  if (tokens.size() != 4) {
    return fail("slash_minus: unexpected token count");
  }
  if (!expect_kind(tokens[0], TokenKind::Slash, "slash_minus t0")) {
    return false;
  }
  if (!expect_kind(tokens[1], TokenKind::Minus, "slash_minus t1")) {
    return false;
  }
  if (!expect_kind(tokens[2], TokenKind::Slash, "slash_minus t2")) {
    return false;
  }
  return expect_kind(tokens[3], TokenKind::EndOfFile, "slash_minus eof");
}

bool case_dot_number_boundary() {
  const std::vector<Token> tokens = lex_all("1.foo 2.5");
  const std::vector<TokenKind> expected = {
      TokenKind::NumericLiteral,
      TokenKind::Dot,
      TokenKind::Identifier,
      TokenKind::NumericLiteral,
      TokenKind::EndOfFile,
  };

  if (tokens.size() != expected.size()) {
    return fail("dot_number_boundary: unexpected token count");
  }

  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (!expect_kind(tokens[i], expected[i], "dot_number_boundary token " + std::to_string(i))) {
      return false;
    }
  }

  if (!expect_lexeme(tokens[0], "1", "dot_number_boundary token0")) {
    return false;
  }
  if (!expect_lexeme(tokens[1], ".", "dot_number_boundary token1")) {
    return false;
  }
  if (!expect_lexeme(tokens[2], "foo", "dot_number_boundary token2")) {
    return false;
  }
  return expect_lexeme(tokens[3], "2.5", "dot_number_boundary token3");
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[1]) != "--case") {
    std::cerr << "usage: lexer_test --case <name>\n";
    return 2;
  }

  const std::string case_name = argv[2];
  const std::unordered_map<std::string, std::function<bool()>> cases = {
      {"keywords", case_keywords},
      {"symbols", case_symbols},
      {"identifiers", case_identifiers},
      {"numeric_literals", case_numeric_literals},
      {"mixed_snippet", case_mixed_snippet},
      {"source_location", case_source_location},
      {"peek", case_peek},
      {"error_token", case_error_token},
      {"slash_minus", case_slash_minus},
      {"dot_number_boundary", case_dot_number_boundary},
  };

  const auto it = cases.find(case_name);
  if (it == cases.end()) {
    std::cerr << "unknown case: " << case_name << "\n";
    return 2;
  }

  return it->second() ? 0 : 1;
}
