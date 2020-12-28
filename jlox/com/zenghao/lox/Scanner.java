/**
 * Responsible for taking a source code string and lexing it into an array of tokens.
 */
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

  // Reserved keywords used by the language
  private static final Map<String, TokenType> reserved;
  static {
    reserved = new HashMap<>();
    reserved.put("and", TokenType.AND);
    reserved.put("class", TokenType.CLASS);
    reserved.put("else", TokenType.ELSE);
    reserved.put("false", TokenType.FALSE);
    reserved.put("for", TokenType.FOR);
    reserved.put("fun", TokenType.FUN);
    reserved.put("if", TokenType.IF);
    reserved.put("nil", TokenType.NIL);
    reserved.put("or", TokenType.OR);
    reserved.put("print", TokenType.PRINT);
    reserved.put("return", TokenType.RETURN);
    reserved.put("super", TokenType.SUPER);
    reserved.put("this", TokenType.THIS);
    reserved.put("true", TokenType.TRUE);
    reserved.put("var", TokenType.VAR);
    reserved.put("while", TokenType.WHILE);
  }

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
    return tokens;
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

      // The start of a string literal
      case '"':
        string();
        break;
      default:
        // digit
        // This is in default case because writing cases for all digits is a pain
        if (isDigit(c)) {
          number();

        }

        // letter or underscore
        else if (isAlpha(c)) {
          identifier();
        }
        // Source file contains a character Lox does not use
        else {
          Lox.error(line, "Unexpected character: " + c);
        }
    }
  }

  /**
   * Parse an identifier.
   */
  private void identifier() {
    while (isAlphaNumeric(peek())) {
      advance();
    }

    String text = source.substring(start, current);
    TokenType type = reserved.get(text);
    type = (type == null) ? TokenType.IDENTIFIER : type;

    addToken(type);
  }

  /**
   * Handling the parsing of a string literal.
   */
  private void string() {
    // Consume chars until the string or file ends
    while (peek() != '"' && !isAtEnd()) {
      // Support multi-line strings
      if (peek() == '\n') {
        line++;
      }
      advance();
    }

    // Ran out of input to parse before string is terminated
    if (isAtEnd()) {
      Lox.error(line, "Unterminated string literal.");
      return;
    }

    // Consume the closing quotation.
    advance();

    // Capture the string literal minus the surrounding quotations.
    String value = source.substring(start + 1, current - 1);
    addToken(TokenType.STRING, value);
  }

  /**
   * Parse a number.
   */
  private void number() {
    // Keep consuming characters until the number ends, or we hit a decimal point
    while (isDigit(peek())) {
      advance();
    }

    // Fractional
    if (peek() == '.' && isDigit(peekNext())) {
      advance();

      while (isDigit(peek())) {
        advance();
      }
    }

    addToken(TokenType.NUMBER, Double.parseDouble(source.substring(start, current)));

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

  /**
   * Look ahead at the next character and do not consume.
   * 
   * @return the next char.
   */
  private char peekNext() {
    if (current + 1 >= source.length()) {
      return '\0';
    }

    return source.charAt(current + 1);
  }

  /**
   * We dont use Character.isDigit() because it allows things like devangari
   * digits
   * 
   * @return whether <c> is a number
   */
  private boolean isDigit(char c) {
    return c >= '0' && c <= '9';
  }

  private boolean isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }

  private boolean isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
  }

  private boolean isAtEnd() {
    return current >= source.length();
  }
}
