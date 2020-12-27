package com.zenghao.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Scanner {
  private final String source;
  private final List<Token> tokens = new ArrayList<>();

  // Offsets
  // first char in the lexeme being scanned
  private int start = 0;
  // Character being parsed
  private int current = 0;
  // The source line <current> is on
  private int line = 1;

  Scanner(String source) {
    this.source = source;
  }

  List<Token> scanTokens() {
    while (!isAtEnd()) {
      // We are at the beginning of the next lexeme
      start = current;
      scanToken();
    }

    // Add an end of file token once done
    tokens.add(new Token(TokenType.EOF, "", null, line));
  }

  private void scanToken() {
    char c = advance();
    switch (c) {
      // Literals
      case '(':
        addToken(TokenType.LEFT_PAREN);
        break;
      case ')':
        addToken(TokenType.RIGHT_PAREN);
        break;
      case '{':
        addToken(TokenType.LEFT_BRACE);
        break;
      case '}':
        addToken(TokenType.RIGHT_BRACE);
        break;
      case ',':
        addToken(TokenType.COMMA);
        break;
      case '.':
        addToken(TokenType.DOT);
        break;
      case '-':
        addToken(TokenType.MINUS);
        break;
      case '+':
        addToken(TokenType.PLUS);
        break;
      case ';':
        addToken(TokenType.SEMICOLON);
        break;
      case '*':
        addToken(TokenType.STAR);
        break;

      // Operators which may be multiple chars
      case '!':
        addToken(match('=') ? TokenType.BANG_EQUAL : TokenType.BANG);
        break;
      case '=':
        addToken(match('=') ? TokenType.EQUAL_EQUAL : TokenType.EQUAL);
        break;
      case '<':
        addToken(match('=') ? TokenType.LESS_EQUAL : TokenType.LESS);
        break;
      case '>':
        addToken(match('=') ? TokenType.GREATER_EQUAL : TokenType.GREATER);
        break;

      // The backslash can either mean division or a line comment
      case '/':
        // Handle comment
        if (match('/')) {
          // Comment reaches until the end of the line, or file
          while (peek() != '\n' && !isAtEnd()) {
            advance();
          }
        }
        // Handle div operator
        else {
          addToken(TokenType.SLASH);
        }
        break;

      // Meaningless characters
      // Ignore all whitespace
      case ' ':
      case '\r':
      case '\t':
        break;

      case '\n':
        line++;
        break;

      // Source file contains a character Lox does not use
      default:
        Lox.error(line, "Unexpected character: " + c);
        break;
    }
  }

  /**
   * Consume the current character and increment.
   * 
   * @return the char being consumed.
   */
  private char advance() {
    current++;
    return source.charAt(current - 1);
  }

  private void addToken(TokenType type) {
    addToken(type, null);
  }

  private void addToken(TokenType type, Object literal) {
    String text = source.substring(start, current);
    tokens.add(new Token(type, text, literal, line));
  }

  /**
   * Consume the current character if it matches <expected>.
   * 
   * @param expected The char we are looking for.
   * @return whether the current char matches.
   */
  private boolean match(char expected) {
    if (isAtEnd()) {
      return false;
    }
    if (source.charAt(current) != expected) {
      return false;
    }

    current++;
    return true;
  }

  /**
   * Look ahead at the current character but do not consume.
   * 
   * @return the current char.
   */
  private char peek() {
    return isAtEnd() ? '\0' : source.charAt(current);
  }

  private boolean isAtEnd() {
    return current >= source.length();
  }
}
