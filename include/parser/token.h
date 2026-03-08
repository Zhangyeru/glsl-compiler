#pragma once

#include <cstddef>
#include <string>

namespace glsl2llvm::parser {

enum class TokenKind {
  EndOfFile,
  Error,

  Identifier,
  NumericLiteral,

  KeywordVoid,
  KeywordBool,
  KeywordInt,
  KeywordUint,
  KeywordFloat,
  KeywordVec2,
  KeywordVec3,
  KeywordVec4,
  KeywordIf,
  KeywordElse,
  KeywordFor,
  KeywordReturn,
  KeywordLayout,
  KeywordIn,
  KeywordBuffer,
  KeywordShared,

  LBrace,
  RBrace,
  LParen,
  RParen,
  LBracket,
  RBracket,
  Semicolon,
  Comma,
  Dot,
  Hash,
  Equal,
  EqualEqual,
  Less,
  Ampersand,
  Plus,
  PlusPlus,
  Minus,
  Star,
  Slash,
};

struct SourceLocation {
  std::size_t offset = 0;
  int line = 1;
  int column = 1;
};

struct Token {
  TokenKind kind = TokenKind::Error;
  SourceLocation location;
  std::string lexeme;
  std::string error_message;
};

const char *to_string(TokenKind kind);

}  // namespace glsl2llvm::parser
