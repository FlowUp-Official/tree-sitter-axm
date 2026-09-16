#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stddef.h>

// External scanner for the `sql_body` token: consumes a `{ ... }` region whose
// braces are balanced only outside SQL strings, dollar-quoted strings and
// comments. Mirrors `scan_sql_body` in
// `crates/axiom-core/src/axm/parser.rs`.
//
// Lexing state is stateless, so nothing needs (de)serializing.

enum TokenType {
  SQL_BODY,
};

void *tree_sitter_axm_external_scanner_create(void) { return NULL; }

void tree_sitter_axm_external_scanner_destroy(void *payload) {}

void tree_sitter_axm_external_scanner_reset(void *payload) {}

unsigned tree_sitter_axm_external_scanner_serialize(void *payload, char *buffer) {
  return 0;
}

void tree_sitter_axm_external_scanner_deserialize(
    void *payload,
    const char *buffer,
    unsigned length) {}

static bool is_ident_char(int32_t c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         c == '_';
}

static bool is_whitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static void skip_whitespace(TSLexer *lexer) {
  while (is_whitespace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }
}

// Handle a single- or double-quoted string body. When `quote` is `'`, applies
// SQL escaping rules: `\X` escapes the next char and `''` is a literal quote.
// The opening quote has already been consumed.
static bool consume_quoted(TSLexer *lexer, int32_t quote) {
  while (!lexer->eof(lexer)) {
    if (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (!lexer->eof(lexer)) {
        lexer->advance(lexer, false);
      }
      continue;
    }
    if (lexer->lookahead == quote) {
      lexer->advance(lexer, false);
      if (lexer->lookahead == quote) {
        lexer->advance(lexer, false);
        continue;
      }
      return true;
    }
    lexer->advance(lexer, false);
  }
  return false; // unterminated
}

// Handle a `--` line comment. Both dashes have been consumed.
static void consume_line_comment(TSLexer *lexer) {
  while (!lexer->eof(lexer) && lexer->lookahead != '\n') {
    lexer->advance(lexer, false);
  }
}

// Handle a `/* */` block comment. The `/` has been consumed and the
// lookahead is the `*`.
static bool consume_block_comment(TSLexer *lexer) {
  lexer->advance(lexer, false); // consume `*`
  while (true) {
    if (lexer->eof(lexer)) {
      return false; // unterminated; consume the rest of the input
    }
    if (lexer->lookahead == '*') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '/') {
        lexer->advance(lexer, false);
        return true;
      }
      continue;
    }
    lexer->advance(lexer, false);
  }
}

// Handle a `$tag$ ... $tag$` dollar-quoted string. The opening `$` is
// consumed on entry. Placeholders such as `$id` or `$1` are not followed by `$`
// and are treated as plain text.
static bool consume_dollar_quote(TSLexer *lexer) {
  enum { MAX_TAG = 128 };
  char tag[MAX_TAG];
  size_t len = 0;

  lexer->advance(lexer, false); // consume the opening `$`
  tag[len++] = '$';
  while (len < MAX_TAG && is_ident_char(lexer->lookahead)) {
    tag[len++] = (char)lexer->lookahead;
    lexer->advance(lexer, false);
  }
  if (lexer->lookahead != '$') {
    return true; // placeholder, not a dollar quote
  }
  tag[len++] = '$';
  lexer->advance(lexer, false); // consume closing `$` of the opening tag

  while (!lexer->eof(lexer)) {
    if (lexer->lookahead != '$') {
      lexer->advance(lexer, false);
      continue;
    }
    // Possibly the start of the closing `$tag$`: consume and compare.
    lexer->advance(lexer, false);
    size_t i = 1;
    bool matched = true;
    while (i < len) {
      if (lexer->eof(lexer)) {
        return false;
      }
      if ((char)lexer->lookahead != tag[i]) {
        matched = false;
        break;
      }
      i++;
      lexer->advance(lexer, false);
    }
    if (matched) {
      return true; // closing `$tag$` consumed
    }
    // The consumed chars are part of the dollar-quoted body; keep scanning.
  }
  return false; // unterminated
}

static bool scan_sql_body(TSLexer *lexer) {
  skip_whitespace(lexer);
  if (lexer->lookahead != '{') {
    return false;
  }

  lexer->advance(lexer, false); // consume opening `{`
  int depth = 1;

  while (true) {
    if (lexer->eof(lexer)) {
      return false; // unterminated SQL body: missing closing `}`
    }

    switch (lexer->lookahead) {
      case '\'':
        lexer->advance(lexer, false); // consume opening quote
        if (!consume_quoted(lexer, '\'')) {
          return false;
        }
        break;
      case '"':
        lexer->advance(lexer, false); // consume opening quote
        if (!consume_quoted(lexer, '"')) {
          return false;
        }
        break;
      case '$':
        if (!consume_dollar_quote(lexer)) {
          return false;
        }
        break;
      case '-':
        lexer->advance(lexer, false);
        if (lexer->lookahead == '-') {
          consume_line_comment(lexer);
        }
        break;
      case '/':
        lexer->advance(lexer, false);
        if (lexer->lookahead == '*') {
          if (!consume_block_comment(lexer)) {
            return false;
          }
        }
        break;
      case '{':
        depth++;
        lexer->advance(lexer, false);
        break;
      case '}':
        depth--;
        lexer->advance(lexer, false);
        if (depth == 0) {
          return true; // token ends just after the closing `}`
        }
        break;
      default:
        lexer->advance(lexer, false);
        break;
    }
  }
}

bool tree_sitter_axm_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols) {
  if (!valid_symbols[SQL_BODY]) {
    return false;
  }
  lexer->result_symbol = SQL_BODY;
  return scan_sql_body(lexer);
}