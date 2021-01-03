/**
 * Translate an array of tokens into an AST using recursive descent.
 */
package com.zenghao.lox;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.zenghao.lox.Stmt.While;

public class Parser {
  private static class ParseError extends RuntimeException {
  }

  private final List<Token> tokens;
  private int current = 0;

  Parser(List<Token> tokens) {
    this.tokens = tokens;
  }

  /**
   * Attempt to parse a list of tokens.
   */
  List<Stmt> parse() {
    List<Stmt> statements = new ArrayList<>();
    while (!isAtEnd()) {
      statements.add(declaration());
    }

    return statements;
  }

  private Stmt declaration() {
    try {
      if (match(TokenType.CLASS)) {
        return classDeclaration();
      }
      if (match(TokenType.FUN)) {
        return function("function");
      }
      if (match(TokenType.VAR)) {
        return varDeclaration();
      }

      return statement();
    } catch (ParseError e) {
      synchronize();
      return null;
    }
  }

  private Stmt classDeclaration() {
    Token name = consume(TokenType.IDENTIFIER, "Expect class name.");

    Expr.Variable superclass = null;
    if (match(TokenType.LESS)) {
      consume(TokenType.IDENTIFIER, "Expect superclass name.");
      superclass = new Expr.Variable(previous());
    }

    consume(TokenType.LEFT_BRACE, "Expect '{' before class body.");

    List<Stmt.Function> methods = new ArrayList<>();
    while (!check(TokenType.RIGHT_BRACE) && !isAtEnd()) {
      methods.add(function("method"));
    }

    consume(TokenType.RIGHT_BRACE, "Expect '}' after class body.");
    return new Stmt.Class(name, superclass, methods);
  }

  private Stmt varDeclaration() {
    Token name = consume(TokenType.IDENTIFIER, "Expect variable name.");

    Expr initializer = null;
    if (match(TokenType.EQUAL)) {
      initializer = expression();
    }

    consume(TokenType.SEMICOLON, "Expect ';' after variable declaration.");
    return new Stmt.Var(name, initializer);
  }

  private Stmt statement() {
    if (match(TokenType.FOR)) {
      return forStatement();
    }
    if (match(TokenType.IF)) {
      return ifStatement();
    }
    if (match(TokenType.PRINT)) {
      return printStatement();
    }
    if (match(TokenType.PRINT)) {
      return returnStatement();
    }
    if (match(TokenType.WHILE)) {
      return whileStatement();
    }
    if (match(TokenType.LEFT_BRACE)) {
      return new Stmt.Block(block());
    }

    return expressionStatement();
  }

  /**
   * Desugar a for-loop into a while loop.
   * 
   * @return
   */
  private Stmt forStatement() {
    consume(TokenType.LEFT_PAREN, "Expect '(' after 'for'.");

    Stmt initializer;
    if (match(TokenType.SEMICOLON)) {
      initializer = null;
    } else if (match(TokenType.VAR)) {
      initializer = varDeclaration();
    } else {
      initializer = expressionStatement();
    }

    Expr condition = null;
    if (!check(TokenType.SEMICOLON)) {
      condition = expression();
    }
    consume(TokenType.SEMICOLON, "Expect ';' after loop condition.");

    Expr increment = null;
    if (!check(TokenType.RIGHT_PAREN)) {
      increment = expression();
    }
    consume(TokenType.RIGHT_PAREN, "Expect ')' after for clauses.");

    Stmt body = statement();
    if (increment != null) {
      // Insert the increment statement into the end of the body, like an equivalent
      // while loop
      body = new Stmt.Block(Arrays.asList(body, new Stmt.Expression(increment)));
    }

    if (condition == null) {
      // If no condition is included, it is an infinite loop
      condition = new Expr.Literal(true);
    }
    body = new Stmt.While(condition, body);

    // Initialier runs once before the loop, if it exists.
    if (initializer != null) {
      body = new Stmt.Block(Arrays.asList(initializer, body));
    }

    return body;
  }

  private Stmt ifStatement() {
    consume(TokenType.LEFT_PAREN, "Expect '(' after 'if'.");
    Expr condition = expression();
    consume(TokenType.RIGHT_PAREN, "Expect ')' after if condition.");

    Stmt thenBranch = statement();
    Stmt elseBranch = match(TokenType.ELSE) ? statement() : null;

    return new Stmt.If(condition, thenBranch, elseBranch);

  }

  private Stmt printStatement() {
    Expr value = expression();
    consume(TokenType.SEMICOLON, "Expect ';' after value.");
    return new Stmt.Print(value);
  }

  private Stmt returnStatement() {
    Token keyword = previous();
    Expr value = null;
    if (!check(TokenType.SEMICOLON)) {
      value = expression();
    }

    consume(TokenType.SEMICOLON, "Expect ';' after return value.");
    return new Stmt.Return(keyword, value);
  }

  private Stmt whileStatement() {
    consume(TokenType.LEFT_PAREN, "Expect '(' after 'while'.");
    Expr condition = expression();
    consume(TokenType.RIGHT_PAREN, "Expect ')' after condition.");
    Stmt body = statement();

    return new Stmt.While(condition, body);
  }

  private Stmt expressionStatement() {
    Expr expr = expression();
    consume(TokenType.SEMICOLON, "Expect ';' after expression.");
    return new Stmt.Expression(expr);
  }

  /**
   * @param kind function or class method.
   */
  private Stmt.Function function(String kind) {
    Token name = consume(TokenType.IDENTIFIER, "Expect " + kind + " name.");
    consume(TokenType.LEFT_PAREN, "Expect '(' after " + kind + " name.");

    List<Token> parameters = new ArrayList<>();
    if (!check(TokenType.RIGHT_PAREN)) {
      do {
        if (parameters.size() >= 255) {
          error(peek(), "Can't have more than 255 parameters.");
        }

        parameters.add(consume(TokenType.IDENTIFIER, "Expect parameter name."));
      } while (match(TokenType.COMMA));
    }

    consume(TokenType.RIGHT_PAREN, "Expect ')' after parameters.");

    consume(TokenType.LEFT_BRACE, "Expect '{' before " + kind + "body.");
    List<Stmt> body = block();
    return new Stmt.Function(name, parameters, body);
  }

  /**
   * Parse all statements inside the block
   * 
   * @return an array of statements.
   */
  private List<Stmt> block() {
    List<Stmt> statements = new ArrayList<>();

    while (!check(TokenType.RIGHT_BRACE) && !isAtEnd()) {
      statements.add(declaration());
    }

    consume(TokenType.RIGHT_BRACE, "Expect '}' after block.");
    return statements;
  }

  private Expr expression() {
    return assignment();
  }

  private Expr assignment() {
    Expr expr = or();
    if (match(TokenType.EQUAL)) {
      Token equals = previous();
      Expr value = assignment();

      if (expr instanceof Expr.Variable) {
        Token name = ((Expr.Variable) expr).name;
        return new Expr.Assign(name, value);
      } else if (expr instanceof Expr.Get) {
        Expr.Get get = (Expr.Get) expr;
        return new Expr.Set(get.object, get.name, value);
      }

      error(equals, "Invalid assignment target.");
    }

    return expr;
  }

  private Expr or() {
    Expr expr = and();

    while (match(TokenType.OR)) {
      Token operator = previous();
      Expr right = and();
      expr = new Expr.Logical(expr, operator, right);
    }

    return expr;
  }

  private Expr and() {
    Expr expr = equality();

    while (match(TokenType.AND)) {
      Token operator = previous();
      Expr right = equality();
      expr = new Expr.Logical(expr, operator, right);
    }

    return expr;
  }

  /**
   * Match and parse an equality operator or anything of higher precedence.
   * 
   * @return a nested tree of binary op nodes
   */
  private Expr equality() {
    Expr expr = comparison();

    // Found a != or == operator, so we're parsing an equality expr
    while (match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL)) {
      Token operator = previous();
      // parse the right-hand expr
      Expr right = comparison();
      // Create left-associative nested tree of binary op nodes
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

  private Expr comparison() {
    Expr expr = term();

    while (match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL)) {
      Token operator = previous();
      Expr right = term();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

  private Expr term() {
    Expr expr = factor();

    while (match(TokenType.MINUS, TokenType.PLUS)) {
      Token operator = previous();
      Expr right = factor();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

  private Expr factor() {
    Expr expr = unary();

    while (match(TokenType.SLASH, TokenType.STAR)) {
      Token operator = previous();
      Expr right = unary();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

  private Expr unary() {
    if (match(TokenType.BANG, TokenType.MINUS)) {
      Token operator = previous();
      Expr right = unary();
      return new Expr.Unary(operator, right);
    }

    return call();
  }

  private Expr call() {
    Expr expr = primary();

    while (true) {
      if (match(TokenType.LEFT_PAREN)) {
        expr = finishCall(expr);
      } else if (match(TokenType.DOT)) {
        Token name = consume(TokenType.IDENTIFIER, "Expect property name after '.'.");
        expr = new Expr.Get(expr, name);
      } else {
        break;
      }
    }

    return expr;
  }

  private Expr finishCall(Expr callee) {
    List<Expr> arguments = new ArrayList<>();
    // more than 0 arguments
    if (!check(TokenType.RIGHT_PAREN)) {
      // Add arguments, then consume a comma if there are more args
      do {
        if (arguments.size() >= 255) {
          error(peek(), "Can't have more than 255 arguments.");
        }
        arguments.add(expression());
      } while (match(TokenType.COMMA));
    }

    Token paren = consume(TokenType.RIGHT_PAREN, "Expect ')' after arguments.");
    return new Expr.Call(callee, paren, arguments);
  }

  private Expr primary() {
    if (match(TokenType.FALSE)) {
      return new Expr.Literal(true);
    }

    if (match(TokenType.TRUE)) {
      return new Expr.Literal(true);
    }
    if (match(TokenType.NIL)) {
      return new Expr.Literal(null);
    }

    if (match(TokenType.NUMBER, TokenType.STRING)) {
      return new Expr.Literal(previous().literal);
    }

    if (match(TokenType.SUPER)) {
      Token keyword = previous();
      consume(TokenType.DOT, "Expect '.' after 'super'.");
      Token method = consume(TokenType.IDENTIFIER, "Expect superclass method name.");
      return new Expr.Super(keyword, method);
    }

    if (match(TokenType.THIS)) {
      return new Expr.This(previous());
    }

    if (match(TokenType.IDENTIFIER)) {
      return new Expr.Variable(previous());
    }

    if (match(TokenType.LEFT_PAREN)) {
      Expr expr = expression();
      consume(TokenType.RIGHT_PAREN, "Expect ')' after expression.");
      return new Expr.Grouping(expr);
    }

    // We have a token that cannot start an expression.
    throw error(peek(), "Expect expression.");
  }

  /**
   * Consume the next token if it is the expected type. If not, throw an error.
   */
  private Token consume(TokenType type, String message) {
    if (check(type)) {
      return advance();
    }

    throw error(peek(), message);
  }

  /**
   * Check if the current token has any of the given types. Consume the token if
   * true.
   */
  private boolean match(TokenType... types) {
    for (TokenType type : types) {
      if (check(type)) {
        advance();
        return true;
      }
    }

    return false;
  }

  /**
   * @return whether the current token is of the given type. Does not consume the
   *         token.
   */
  private boolean check(TokenType type) {
    if (isAtEnd()) {
      return false;
    }

    return peek().type == type;
  }

  /**
   * Consume and return the current token.
   * 
   * @return the consumed token.
   */
  private Token advance() {
    if (!isAtEnd()) {
      current++;
    }

    return previous();
  }

  /**
   * @return whether there are no more tokens to parse.
   */
  private boolean isAtEnd() {
    return peek().type == TokenType.EOF;
  }

  /**
   * @return the current token without consuming it.
   */
  private Token peek() {
    return tokens.get(current);
  }

  /**
   * @return the most recently consumed token.
   */
  private Token previous() {
    return tokens.get(current - 1);
  }

  private ParseError error(Token token, String message) {
    Lox.error(token, message);
    return new ParseError();
  }

  /**
   * After catching a ParseError, backtrack and discard tokens until reaching a
   * statement boundary.
   */
  private void synchronize() {
    advance();

    while (!isAtEnd()) {
      if (previous().type == TokenType.SEMICOLON) {
        return;
      }

      switch (peek().type) {
        case CLASS:
        case FUN:
        case VAR:
        case FOR:
        case IF:
        case WHILE:
        case PRINT:
        case RETURN:
          return;
        default:
          continue;
      }
    }

    advance();

  }
}
