package com.zenghao.lox;

import java.util.HashMap;
import java.util.Map;

class Environment {
  private final Map<String, Object> values = new HashMap<>();

  /**
   * Define a new variable <name> binded to <value>. Variable statements can be
   * used to re-define existing variables.
   */
  void define(String name, Object value) {
    values.put(name, value);
  }

  /**
   * Assignment is NOT allowed to create a new variable.
   */
  void assign(Token name, Object value) {
    if (values.containsKey(name.lexeme)) {
      values.put(name.lexeme, value);
      return;
    }

    throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }

  Object get(Token name) {
    if (values.containsKey(name.lexeme)) {
      return values.get(name.lexeme);
    }

    throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }
}
