#pragma once

#include "parser/token.h"

#include <string>
#include <string_view>

namespace glsl2llvm::parser {

class Lexer {
 public:
  Lexer(std::string_view source, std::string source_name = "<memory>");

  Token next();
  Token peek();

 private:
  Token lex_token();
  void skip_whitespace();

  bool at_end() const;
  char current_char() const;
  char lookahead(std::size_t n) const;
  void advance();

  Token make_simple_token(TokenKind kind, SourceLocation location, char ch) const;
  Token lex_identifier_or_keyword();
  Token lex_number();
  Token lex_error(std::string message);

  static TokenKind keyword_kind(std::string_view text);

  std::string source_;
  std::string source_name_;
  std::size_t index_ = 0;
  int line_ = 1;
  int column_ = 1;

  bool has_peeked_ = false;
  Token peeked_token_;
};

}  // namespace glsl2llvm::parser
