#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include "scanner.h"

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panic;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

// Function ptr that takes a scanner and parser and returns nothing
typedef void (*ParseFn)(Scanner *, Parser *);
typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static void initParser(Parser *parser) {
  parser->hadError = false;
  parser->panic = false;
}

Chunk *compilingChunk;

static Chunk *currentChunk() { return compilingChunk; }

static void errorAt(Parser *parser, Token *token, const char *message) {
  if (parser->panic) {
    return;
  }

  parser->panic = true;

  fprintf(stderr, "[line %d] Error", token->line);

  switch (token->type) {
  case TOKEN_EOF:
    fprintf(stderr, " at end");
    break;
  case TOKEN_ERROR:
    break;
  default:
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser->hadError = true;
}

static void error(Parser *parser, const char *message) {
  errorAt(parser, &parser->previous, message);
}
static void errorAtCurrent(Parser *parser, const char *message) {
  errorAt(parser, &parser->current, message);
}

/**
 * Scan the next token.
 * If encountering an error, keep scanning tokens until we reach a valid token.
 */
static void advance(Scanner *scanner, Parser *parser) {
  parser->previous = parser->current;

  while (true) {
    parser->current = scanToken(scanner);
    if (parser->current.type != TOKEN_ERROR) {
      break;
    }

    errorAtCurrent(parser, parser->current.start);
  }
}

static void consume(Scanner *scanner, Parser *parser, TokenType type,
                    const char *message) {
  if (parser->current.type == type) {
    advance(scanner, parser);
    return;
  }

  errorAtCurrent(parser, message);
}

static void emitByte(Parser *parser, uint8_t byte) {
  writeChunk(currentChunk(), byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2) {
  emitByte(parser, byte1);
  emitByte(parser, byte2);
}

static void emitReturn(Parser *parser) { emitByte(parser, OP_RETURN); }

static uint8_t makeConstant(Parser *parser, Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    error(parser, "Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}
static void emitConstant(Parser *parser, Value value) {
  emitBytes(parser, OP_CONSTANT, makeConstant(parser, value));
}

static void endCompiler(Parser *parser) {
  emitReturn(parser);

#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}

// The table is further down
ParseRule rules[];
static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void parsePrecedence(Scanner *scanner, Parser *parser,
                            Precedence precedence) {
  advance(scanner, parser);
  ParseFn prefixRule = getRule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    error(parser, "Expect expression.");
    return;
  }

  prefixRule(scanner, parser);

  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(scanner, parser);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(scanner, parser);
  }
}

static void binary(Scanner *scanner, Parser *parser) {
  TokenType operatorType = parser->previous.type;

  // Compile right operand
  ParseRule *rule = getRule(operatorType);
  parsePrecedence(scanner, parser, ((Precedence)(rule->precedence + 1)));

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitBytes(parser, OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(parser, OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(parser, OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(parser, OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(parser, OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(parser, OP_GREATER, OP_NOT);
    break;

  case TOKEN_PLUS:
    emitByte(parser, OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(parser, OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(parser, OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(parser, OP_DIVIDE);
    break;
  default:
    return; // Unreachable.
  }
}

static void literal(Scanner *scanner, Parser *parser) {
  switch (parser->previous.type) {
  case TOKEN_FALSE:
    emitByte(parser, OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(parser, OP_NIL);
    break;
  case TOKEN_TRUE:
    emitByte(parser, OP_TRUE);
    break;
  default:
    return;
  }
}

static void expression(Scanner *scanner, Parser *parser) {
  parsePrecedence(scanner, parser, PREC_ASSIGNMENT);
}

static void number(Scanner *scanner, Parser *parser) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, NUMBER_VAL(value));
}

static void unary(Scanner *scanner, Parser *parser) {
  TokenType operatorType = parser->previous.type;

  parsePrecedence(scanner, parser, PREC_UNARY);

  switch (operatorType) {
  case TOKEN_BANG:
    emitByte(parser, OP_NOT);
    break;
  case TOKEN_MINUS:
    emitByte(parser, OP_NEGATE);
    break;
  default:
    return;
  }
}

static void grouping(Scanner *scanner, Parser *parser) {
  expression(scanner, parser);
  consume(scanner, parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

// An array where the index of a token enum member is a parseRule
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

bool compile(const char *source, Chunk *chunk) {
  Scanner scanner;
  initScanner(&scanner, source);
  Parser parser;
  initParser(&parser);
  compilingChunk = chunk;

  advance(&scanner, &parser);
  expression(&scanner, &parser);
  consume(&scanner, &parser, TOKEN_EOF, "Expect end of expression.");

  endCompiler(&parser);

  return !parser.hadError;
}
