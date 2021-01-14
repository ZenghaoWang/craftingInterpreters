#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include "scanner.h"

typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

typedef struct {
  Token current;
  Token previous;
  VM *vm;
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
typedef void (*ParseFn)(Scanner *, Parser *, Compiler *, bool canAssign);
typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static void initParser(Parser *parser, VM *vm) {
  parser->hadError = false;
  parser->panic = false;
  parser->vm = vm;
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

static bool check(Parser *parser, TokenType type) {
  return parser->current.type == type;
}

static bool match(Scanner *scanner, Parser *parser, TokenType type) {
  if (!check(parser, type)) {
    return false;
  }

  advance(scanner, parser);
  return true;
}

static void emitByte(Parser *parser, uint8_t byte) {
  writeChunk(currentChunk(), byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2) {
  emitByte(parser, byte1);
  emitByte(parser, byte2);
}

static int emitJump(Parser *parser, uint8_t opCode) {
  emitByte(parser, opCode);
  emitBytes(parser, 0xff, 0xff);
  return currentChunk()->count - 2;
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

static void patchJump(Parser *parser, int offset) {
  // Number of bytes to jump over
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error(parser, "Too much code to jump over.");
  }

  // Insert a 16 bit unsigned integer
  // (jump >> 8) & 0xff is the 8 leftmost bits of jumpn
  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  // jump & 0xff is the 8 rightmost bits of jump
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
}

static void endCompiler(Parser *parser) {
  emitReturn(parser);

#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}

static void beginScope(Compiler *compiler) { compiler->scopeDepth++; }

static void endScope(Parser *parser, Compiler *compiler) {
  compiler->scopeDepth--;
  // Pop all local variables going out of scope
  while (compiler->localCount > 0 &&
         compiler->locals[compiler->localCount - 1].depth >
             compiler->scopeDepth) {
    emitByte(parser, OP_POP);
    compiler->localCount--;
  }
}

// The table is further down
ParseRule rules[];
static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void parsePrecedence(Scanner *scanner, Parser *parser,
                            Compiler *compiler, Precedence precedence) {
  advance(scanner, parser);
  ParseFn prefixRule = getRule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    error(parser, "Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(scanner, parser, compiler, canAssign);

  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(scanner, parser);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(scanner, parser, compiler, canAssign);
  }

  if (canAssign && match(scanner, parser, TOKEN_EQUAL)) {
    error(parser, "Invalid assignment target.");
  }
}

static uint8_t identifierConstant(Parser *parser, Token *name) {
  return makeConstant(
      parser, OBJ_VAL(copyString(parser->vm, name->start, name->length)));
}

/**
 * Store a local variable at the end of the array.
 */
static void addLocal(Parser *parser, Compiler *compiler, Token name) {
  if (compiler->localCount == UINT8_COUNT) {
    error(parser, "Too many local variables in function.");
  }

  Local *local = &compiler->locals[compiler->localCount++];
  local->name = name;
  // Sentinal value to mark an uninitialized variable.
  local->depth = -1;
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length) {
    return false;
  }

  return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * Return the index of the local variable, or -1 if it is a global.
 */
static int resolveLocal(Parser *parser, Compiler *compiler, Token *name) {
  // Walk through the variables currently in scope, starting with the most
  // recently declared, to ensure that variables shadow variables in
  // surrounding scopes.
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = compiler->locals + i;
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error(parser, "Can't read local variable in its own initializer.");
      }
      return i;
    }
  }

  // Not found, must be a global
  return -1;
}

static void declareVariable(Parser *parser, Compiler *compiler) {
  if (compiler->scopeDepth == 0) {
    return;
  }

  Token *name = &parser->previous;

  // Check for redeclared variables in same scope
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = compiler->locals + i;

    if (local->depth != -1 && local->depth < compiler->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error(parser, "Already variable with this name in this scope.");
    }
  }
  addLocal(parser, compiler, *name);
}

static uint8_t parseVariable(Scanner *scanner, Parser *parser,
                             Compiler *compiler, const char *errorMessage) {
  consume(scanner, parser, TOKEN_IDENTIFIER, errorMessage);

  declareVariable(parser, compiler);
  if (compiler->scopeDepth > 0) {
    return 0;
  }
  return identifierConstant(parser, &parser->previous);
}

static void markInitialized(Compiler *compiler) {
  compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static void defineVariable(Parser *parser, Compiler *compiler,
                           uint8_t global_idx) {
  if (compiler->scopeDepth > 0) {
    markInitialized(compiler);
    return;
  }

  emitBytes(parser, OP_DEFINE_GLOBAL, global_idx);
}

static void binary(Scanner *scanner, Parser *parser, Compiler *compiler,
                   bool canAssign) {
  TokenType operatorType = parser->previous.type;

  // Compile right operand
  ParseRule *rule = getRule(operatorType);
  parsePrecedence(scanner, parser, compiler,
                  ((Precedence)(rule->precedence + 1)));

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

static void literal(Scanner *scanner, Parser *parser, Compiler *compiler,
                    bool canAssign) {
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

static void expression(Scanner *scanner, Parser *parser, Compiler *compiler) {
  parsePrecedence(scanner, parser, compiler, PREC_ASSIGNMENT);
}

static void statement(Scanner *scanner, Parser *parser, Compiler *compiler);
static void declaration(Scanner *scanner, Parser *parser, Compiler *compiler);

static void block(Scanner *scanner, Parser *parser, Compiler *compiler) {
  while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
    declaration(scanner, parser, compiler);
  }

  consume(scanner, parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration(Scanner *scanner, Parser *parser,
                           Compiler *compiler) {
  uint8_t global_idx =
      parseVariable(scanner, parser, compiler, "Expect variable name.");

  if (match(scanner, parser, TOKEN_EQUAL)) {
    expression(scanner, parser, compiler);
  } else {
    emitByte(parser, OP_NIL);
  }

  consume(scanner, parser, TOKEN_SEMICOLON,
          "Expect ';' after variable declaration.");

  defineVariable(parser, compiler, global_idx);
}

static void expressionStatement(Scanner *scanner, Parser *parser,
                                Compiler *compiler) {
  expression(scanner, parser, compiler);
  consume(scanner, parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(parser, OP_POP);
}

static void ifStatement(Scanner *scanner, Parser *parser, Compiler *compiler) {
  // At runtime, this leaves the condition value on top of the stack.
  consume(scanner, parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression(scanner, parser, compiler);
  consume(scanner, parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(parser, OP_JUMP_IF_FALSE);
  // Pop the condition value
  emitByte(parser, OP_POP);
  // Then branch statement, which can be skipped by the jump
  statement(scanner, parser, compiler);

  // This instruction will be emitted if the then branch is executed, causing
  // the vm to jump past the else block no matter what.
  int elseJump = emitJump(parser, OP_JUMP);

  // Add an operand to the jump instruction telling it how far to jump.
  patchJump(parser, thenJump);

  emitByte(parser, OP_POP);
  if (match(scanner, parser, TOKEN_ELSE)) {
    statement(scanner, parser, compiler);
  }

  patchJump(parser, elseJump);
}

static void printStatement(Scanner *scanner, Parser *parser,
                           Compiler *compiler) {
  expression(scanner, parser, compiler);
  consume(scanner, parser, TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(parser, OP_PRINT);
}

static void synchronize(Scanner *scanner, Parser *parser) {
  parser->panic = false;

  while (parser->current.type != TOKEN_EOF) {
    if (parser->previous.type == TOKEN_SEMICOLON) {
      return;
    }

    switch (parser->current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:
      break;
    }

    advance(scanner, parser);
  }
}

static void declaration(Scanner *scanner, Parser *parser, Compiler *compiler) {
  if (match(scanner, parser, TOKEN_VAR)) {
    varDeclaration(scanner, parser, compiler);
  } else {
    statement(scanner, parser, compiler);
  }

  if (parser->panic) {
    synchronize(scanner, parser);
  }
}

static void statement(Scanner *scanner, Parser *parser, Compiler *compiler) {
  if (match(scanner, parser, TOKEN_PRINT)) {
    printStatement(scanner, parser, compiler);
  } else if (match(scanner, parser, TOKEN_IF)) {
    ifStatement(scanner, parser, compiler);
  } else if (match(scanner, parser, TOKEN_LEFT_BRACE)) {
    beginScope(compiler);
    block(scanner, parser, compiler);
    endScope(parser, compiler);
  } else {
    expressionStatement(scanner, parser, compiler);
  }
}

static void number(Scanner *scanner, Parser *parser, Compiler *compiler,
                   bool canAssign) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, NUMBER_VAL(value));
}

static void string(Scanner *scanner, Parser *parser, Compiler *compiler,
                   bool canAssign) {
  emitConstant(parser,
               OBJ_VAL(copyString(parser->vm, parser->previous.start + 1,
                                  parser->previous.length - 2)));
}

static void namedVariable(Scanner *scanner, Parser *parser, Compiler *compiler,
                          Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(parser, compiler, &name);

  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    arg = identifierConstant(parser, &name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(scanner, parser, TOKEN_EQUAL)) {
    expression(scanner, parser, compiler);
    emitBytes(parser, setOp, (uint8_t)arg);
  } else {
    emitBytes(parser, getOp, (uint8_t)arg);
  }
}

static void variable(Scanner *scanner, Parser *parser, Compiler *compiler,
                     bool canAssign) {
  namedVariable(scanner, parser, compiler, parser->previous, canAssign);
}

static void unary(Scanner *scanner, Parser *parser, Compiler *compiler,
                  bool canAssign) {
  TokenType operatorType = parser->previous.type;

  parsePrecedence(scanner, parser, compiler, PREC_UNARY);

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

static void grouping(Scanner *scanner, Parser *parser, Compiler *compiler,
                     bool canAssign) {
  expression(scanner, parser, compiler);
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
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
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

bool compile(VM *vm, const char *source, Chunk *chunk) {
  Scanner scanner;
  initScanner(&scanner, source);
  Parser parser;
  initParser(&parser, vm);
  Compiler compiler;
  initCompiler(&compiler);
  compilingChunk = chunk;

  advance(&scanner, &parser);
  while (!match(&scanner, &parser, TOKEN_EOF)) {
    declaration(&scanner, &parser, &compiler);
  }

  endCompiler(&parser);

  return !parser.hadError;
}
