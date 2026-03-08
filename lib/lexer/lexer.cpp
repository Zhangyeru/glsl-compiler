#include "parser/lexer.h"

#include <cctype>
#include <utility>

namespace glsl2llvm::parser {

namespace {

bool is_identifier_start(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalpha(uch) || ch == '_';
}

bool is_identifier_continue(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalnum(uch) || ch == '_';
}

bool is_digit(char ch) {
  return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

}  // namespace

const char *to_string(TokenKind kind) {
  switch (kind) {
    case TokenKind::EndOfFile:
      return "EndOfFile";
    case TokenKind::Error:
      return "Error";
    case TokenKind::Identifier:
      return "Identifier";
    case TokenKind::NumericLiteral:
      return "NumericLiteral";
    case TokenKind::KeywordVoid:
      return "KeywordVoid";
    case TokenKind::KeywordBool:
      return "KeywordBool";
    case TokenKind::KeywordInt:
      return "KeywordInt";
    case TokenKind::KeywordUint:
      return "KeywordUint";
    case TokenKind::KeywordFloat:
      return "KeywordFloat";
    case TokenKind::KeywordVec2:
      return "KeywordVec2";
    case TokenKind::KeywordVec3:
      return "KeywordVec3";
    case TokenKind::KeywordVec4:
      return "KeywordVec4";
    case TokenKind::KeywordIf:
      return "KeywordIf";
    case TokenKind::KeywordElse:
      return "KeywordElse";
    case TokenKind::KeywordFor:
      return "KeywordFor";
    case TokenKind::KeywordReturn:
      return "KeywordReturn";
    case TokenKind::KeywordLayout:
      return "KeywordLayout";
    case TokenKind::KeywordIn:
      return "KeywordIn";
    case TokenKind::KeywordBuffer:
      return "KeywordBuffer";
    case TokenKind::KeywordShared:
      return "KeywordShared";
    case TokenKind::LBrace:
      return "LBrace";
    case TokenKind::RBrace:
      return "RBrace";
    case TokenKind::LParen:
      return "LParen";
    case TokenKind::RParen:
      return "RParen";
    case TokenKind::LBracket:
      return "LBracket";
    case TokenKind::RBracket:
      return "RBracket";
    case TokenKind::Semicolon:
      return "Semicolon";
    case TokenKind::Comma:
      return "Comma";
    case TokenKind::Dot:
      return "Dot";
    case TokenKind::Hash:
      return "Hash";
    case TokenKind::Equal:
      return "Equal";
    case TokenKind::EqualEqual:
      return "EqualEqual";
    case TokenKind::Less:
      return "Less";
    case TokenKind::Ampersand:
      return "Ampersand";
    case TokenKind::Plus:
      return "Plus";
    case TokenKind::PlusPlus:
      return "PlusPlus";
    case TokenKind::Minus:
      return "Minus";
    case TokenKind::Star:
      return "Star";
    case TokenKind::Slash:
      return "Slash";
  }

  return "Unknown";
}

Lexer::Lexer(std::string_view source, std::string source_name)
    : source_(source), source_name_(std::move(source_name)) {}

Token Lexer::next() {
  if (has_peeked_) {
    has_peeked_ = false;
    return peeked_token_;
  }
  return lex_token();
}

Token Lexer::peek() {
  if (!has_peeked_) {
    peeked_token_ = lex_token();
    has_peeked_ = true;
  }
  return peeked_token_;
}

Token Lexer::lex_token() {
  skip_whitespace();

  const SourceLocation location{index_, line_, column_};
  if (at_end()) {
    Token token;
    token.kind = TokenKind::EndOfFile;
    token.location = location;
    return token;
  }

  const char ch = current_char();

  if (is_identifier_start(ch)) {
    return lex_identifier_or_keyword();
  }

  if (is_digit(ch)) {
    return lex_number();
  }

  switch (ch) {
    case '{':
      advance();
      return make_simple_token(TokenKind::LBrace, location, ch);
    case '}':
      advance();
      return make_simple_token(TokenKind::RBrace, location, ch);
    case '(':
      advance();
      return make_simple_token(TokenKind::LParen, location, ch);
    case ')':
      advance();
      return make_simple_token(TokenKind::RParen, location, ch);
    case '[':
      advance();
      return make_simple_token(TokenKind::LBracket, location, ch);
    case ']':
      advance();
      return make_simple_token(TokenKind::RBracket, location, ch);
    case ';':
      advance();
      return make_simple_token(TokenKind::Semicolon, location, ch);
    case ',':
      advance();
      return make_simple_token(TokenKind::Comma, location, ch);
    case '.':
      advance();
      return make_simple_token(TokenKind::Dot, location, ch);
    case '#':
      advance();
      return make_simple_token(TokenKind::Hash, location, ch);
    case '=':
      if (lookahead(1) == '=') {
        advance();
        advance();
        Token token;
        token.kind = TokenKind::EqualEqual;
        token.location = location;
        token.lexeme = "==";
        return token;
      }
      advance();
      return make_simple_token(TokenKind::Equal, location, ch);
    case '<':
      advance();
      return make_simple_token(TokenKind::Less, location, ch);
    case '&':
      advance();
      return make_simple_token(TokenKind::Ampersand, location, ch);
    case '+':
      if (lookahead(1) == '+') {
        advance();
        advance();
        Token token;
        token.kind = TokenKind::PlusPlus;
        token.location = location;
        token.lexeme = "++";
        return token;
      }
      advance();
      return make_simple_token(TokenKind::Plus, location, ch);
    case '-':
      advance();
      return make_simple_token(TokenKind::Minus, location, ch);
    case '*':
      advance();
      return make_simple_token(TokenKind::Star, location, ch);
    case '/':
      advance();
      return make_simple_token(TokenKind::Slash, location, ch);
    default:
      return lex_error("unexpected character");
  }
}

void Lexer::skip_whitespace() {
  while (!at_end()) {
    const char ch = current_char();
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
      advance();
      continue;
    }
    break;
  }
}

bool Lexer::at_end() const {
  return index_ >= source_.size();
}

char Lexer::current_char() const {
  return source_[index_];
}

char Lexer::lookahead(std::size_t n) const {
  const std::size_t target = index_ + n;
  if (target >= source_.size()) {
    return '\0';
  }
  return source_[target];
}

void Lexer::advance() {
  if (at_end()) {
    return;
  }

  if (source_[index_] == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }

  ++index_;
}

Token Lexer::make_simple_token(TokenKind kind, SourceLocation location, char ch) const {
  Token token;
  token.kind = kind;
  token.location = location;
  token.lexeme.assign(1, ch);
  return token;
}

Token Lexer::lex_identifier_or_keyword() {
  const SourceLocation location{index_, line_, column_};
  const std::size_t start = index_;

  advance();
  while (!at_end() && is_identifier_continue(current_char())) {
    advance();
  }

  const std::string lexeme = source_.substr(start, index_ - start);

  Token token;
  token.location = location;
  token.lexeme = lexeme;
  token.kind = keyword_kind(lexeme);
  return token;
}

Token Lexer::lex_number() {
  const SourceLocation location{index_, line_, column_};
  const std::size_t start = index_;

  while (!at_end() && is_digit(current_char())) {
    advance();
  }

  // Keep '.' as a symbol unless it starts a clear fractional suffix.
  if (!at_end() && current_char() == '.' && is_digit(lookahead(1))) {
    advance();
    while (!at_end() && is_digit(current_char())) {
      advance();
    }
  }

  if (!at_end() && (current_char() == 'u' || current_char() == 'U')) {
    advance();
  }

  Token token;
  token.kind = TokenKind::NumericLiteral;
  token.location = location;
  token.lexeme = source_.substr(start, index_ - start);
  return token;
}

Token Lexer::lex_error(std::string message) {
  const SourceLocation location{index_, line_, column_};
  const char ch = current_char();
  advance();

  Token token;
  token.kind = TokenKind::Error;
  token.location = location;
  token.lexeme.assign(1, ch);
  token.error_message = std::move(message);
  return token;
}

TokenKind Lexer::keyword_kind(std::string_view text) {
  if (text == "void") {
    return TokenKind::KeywordVoid;
  }
  if (text == "bool") {
    return TokenKind::KeywordBool;
  }
  if (text == "int") {
    return TokenKind::KeywordInt;
  }
  if (text == "uint") {
    return TokenKind::KeywordUint;
  }
  if (text == "float") {
    return TokenKind::KeywordFloat;
  }
  if (text == "vec2") {
    return TokenKind::KeywordVec2;
  }
  if (text == "vec3") {
    return TokenKind::KeywordVec3;
  }
  if (text == "vec4") {
    return TokenKind::KeywordVec4;
  }
  if (text == "if") {
    return TokenKind::KeywordIf;
  }
  if (text == "else") {
    return TokenKind::KeywordElse;
  }
  if (text == "for") {
    return TokenKind::KeywordFor;
  }
  if (text == "return") {
    return TokenKind::KeywordReturn;
  }
  if (text == "layout") {
    return TokenKind::KeywordLayout;
  }
  if (text == "in") {
    return TokenKind::KeywordIn;
  }
  if (text == "buffer") {
    return TokenKind::KeywordBuffer;
  }
  if (text == "shared") {
    return TokenKind::KeywordShared;
  }
  return TokenKind::Identifier;
}

}  // namespace glsl2llvm::parser
