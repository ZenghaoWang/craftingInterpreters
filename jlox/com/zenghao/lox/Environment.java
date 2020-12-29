package com.zenghao.lox;

import java.util.HashMap;
import java.util.Map;

class Environment {
  /**
   * The environment of the immediately enclosing scope.
   */
  final Environment enclosing;
  private final Map<String, Object> values = new HashMap<>();

  /**
   * Constructor for global scope.
   */
  Environment() {
    enclosing = null;
  }

  /**
   * Constructor for block scope.
   */
  Environment(Environment enclosing) {
    this.enclosing = enclosing;
  }

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

    // If variable not found in scope, recursively check enclosing scope
    if (enclosing != null) {
      enclosing.assign(name, value);
      return;
    }

    throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }

  Object get(Token name) {
    if (values.containsKey(name.lexeme)) {
      return values.get(name.lexeme);
    }

    // If variable isn't found in local scope, keep looking in enclosing scopes.
    if (enclosing != null) {
      return enclosing.get(name);
    }

    throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }
}
