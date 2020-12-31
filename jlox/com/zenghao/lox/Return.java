package com.zenghao.lox;

/**
 * We use a custom exception to jump up the call stack when we encounter a
 * return statement inside a function.
 */
class Return extends RuntimeException {
  final Object value;

  Return(Object value) {
    this.value = value;
  }
}
