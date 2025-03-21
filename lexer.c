#include "lexer.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SINGLE_TOK(t) ((Token){.type = t})

String TOKEN_TYPE_MAP[] = {[T_DELIMITOR_LEFT] = STRING("T_DELIMITOR_LEFT"),
                           [T_DELIMITOR_RIGHT] = STRING("T_DELIMITOR_RIGHT"),
                           [T_BOOLEAN] = STRING("T_BOOLEAN"),
                           [T_STRING] = STRING("T_STRING"),
                           [T_NUMBER] = STRING("T_NUMBER"),
                           [T_IDENT] = STRING("T_IDENT"),
                           [T_EOF] = STRING("T_EOF")};

void Token_destroy(Token *token) {
  if (token == NULL)
    return;

  switch (token->type) {
  case T_IDENT:
  case T_STRING:
    free(token->string.p);
    break;
  default:
    return;
  }
}

void Token_debug(Token *token) {
  printf("[%s]", String_to(&TOKEN_TYPE_MAP[token->type]));
  switch (token->type) {
  case T_NUMBER:
    printf("(%f)", token->number);
    break;
  case T_STRING:
  case T_IDENT:
    printf("('%s')", String_to(&token->string));
    break;
  case T_BOOLEAN:
    printf("(%s)", token->boolean ? "true" : "false");
    break;
  default:
    break;
  }
  putc('\n', stdout);
}

Lexer Lexer_new(String input) {
  return (Lexer){
      .input = input,
      .pos = 0,
  };
}

static boolean at_end(Lexer *l) { return l->pos >= l->input.len; }
static char cur(Lexer *l) { return String_get(&l->input, l->pos); }
static boolean is_whitespace(char cc) {
  return cc == ' ' || cc == '\n' || cc == '\t';
}
static boolean is_ident(char cc) {
  return (cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z') || cc == '_';
}

static void advance(Lexer *l) {
  do {
    if (l->pos < l->input.len)
      l->pos++;
  } while (is_whitespace(cur(l)));
}

static void skip_whitespace(Lexer *l) {
  while (is_whitespace(cur(l))) {
    l->pos++;
  }
}

static Token num(Lexer *l) {
  size_t start = l->pos;
  for (char cc = cur(l); cc > 0 && ((cc >= '0' && cc <= '9') || cc == '.');
       l->pos++, cc = cur(l))
    ;
  String s = String_slice(&l->input, start, l->pos);
  double d = strtod(String_to(&s), NULL);
  free(String_to(&s));
  skip_whitespace(l);
  return (Token){
      .type = T_NUMBER,
      .number = d,
  };
}

static Token string(Lexer *l) {
  // skip "
  advance(l);
  size_t start = l->pos;
  for (char cc = cur(l); cc > 0 && cc != '"'; l->pos++, cc = cur(l))
    ;
  if (cur(l) != '"') {
    fprintf(stderr, "Unterminated string");
    return SINGLE_TOK(T_EOF);
  }
  String s = String_slice(&l->input, start, l->pos);
  // skip "
  advance(l);
  return (Token){
      .type = T_STRING,
      .string = s,
  };
}

static Token ident(Lexer *l) {
  size_t start = l->pos;
  for (char cc = cur(l); cc > 0 && is_ident(cc); l->pos++, cc = cur(l))
    ;
  String s = String_slice(&l->input, start, l->pos);
  skip_whitespace(l);
  if (String_eq(&s, &STRING("true"))) {
    free(s.p);
    return (Token){
        .type = T_BOOLEAN,
        .boolean = true,
    };
  } else if (String_eq(&s, &STRING("false"))) {
    free(s.p);
    return (Token){
        .type = T_BOOLEAN,
        .boolean = false,
    };
  } else {
    return (Token){
        .type = T_IDENT,
        .string = s,
    };
  }
}

Token Lexer_next(Lexer *l) {
  if (at_end(l)) {
    return SINGLE_TOK(T_EOF);
  }
  char cc = cur(l);
  switch (cc) {
  case ';':
    for (cc = cur(l); cc > 0 && cc != '\n'; l->pos++, cc = cur(l))
      ;
    advance(l);
    return Lexer_next(l);
  case '"':
    return string(l);
  case '(':
    advance(l);
    return SINGLE_TOK(T_DELIMITOR_LEFT);
  case ')':
    advance(l);
    return SINGLE_TOK(T_DELIMITOR_RIGHT);
    // EOF case
  case -1:
    advance(l);
    return SINGLE_TOK(T_EOF);
  default:
    if ((cc >= '0' && cc <= '9')) {
      return num(l);
    } else if (is_ident(cc)) {
      return ident(l);
    }
    fprintf(stderr, "Unkown token '%c' at %s\n", cur(l), l->input.p + l->pos);
    advance(l);
    return SINGLE_TOK(T_EOF);
  }
}

#undef SINGLE_TOK
