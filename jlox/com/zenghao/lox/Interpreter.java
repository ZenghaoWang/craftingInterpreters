package com.zenghao.lox;

public class Interpreter implements Expr.Visitor<Object> {
  @Override
  public Object visitLiteralExpr(Expr.Literal expr) {
    return expr.value;
  }

  @Override
  public Object visitGroupingExpr(Expr.Grouping expr) {
    return evaluate(expr.expression);
  }

  @Override
  public Object visitUnaryExpr(Expr.Unary expr) {
    Object right = evaluate(expr.right);

    switch (expr.operator.type) {
      case MINUS:
        return -(double) right;
      case BANG:
        return !isTruthy(right);
      default:
        return null;
    }
  }

  @Override
  public Object visitBinaryExpr(Expr.Binary expr) {
    Object left = evaluate(expr.left);
    Object right = evaluate(expr.right);

    switch (expr.operator.type) {
      // Equality
      case BANG_EQUAL:
        return !isEqual(left, right);
      case EQUAL_EQUAL:
        return isEqual(left, right);
      // Comparison operators
      case GREATER:
        return (double) left > (double) right;
      case GREATER_EQUAL:
        return (double) left >= (double) right;
      case LESS:
        return (double) left < (double) right;
      case LESS_EQUAL:
        return (double) left <= (double) right;

      // Arithmetic operators
      case MINUS:
        return (double) left - (double) right;
      case SLASH:
        return (double) left / (double) right;
      case STAR:
        return (double) left * (double) right;
      // + operator is overloaded for addition and string concatenation
      case PLUS:
        if (left instanceof Double && right instanceof Double) {
          return (double) left + (double) right;
        }
        if (right instanceof String && right instanceof String) {
          return (String) left + (String) right;
        }
        break;
      default:
        return null;
    }

    // unreachable, theoretically
    return null;
  }

  /**
   * Direct the Interpreter to the correct method to call according to the type of
   * expr
   * 
   * @return the evaluated expression.
   */
  private Object evaluate(Expr expr) {
    return expr.accept(this);
  }

  /**
   * No implicit conversions (fuck you javascript)
   * 
   * @return whether two Lox objects are equal
   */
  private boolean isEqual(Object a, Object b) {
    if (a == null && b == null) {
      return true;
    }

    if (a == null) {
      return false;
    }

    return a.equals(b);
  }

  /**
   * Determine the truthiness of an object. false and nil are falsey, everything
   * else is truthy.
   */
  private boolean isTruthy(Object object) {
    if (object == null) {
      return false;
    }

    if (object instanceof Boolean) {
      return (boolean) object;
    }

    return true;
  }
}
