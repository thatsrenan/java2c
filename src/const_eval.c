#include "const_eval.h"

#include <stdint.h>

static int is_real(Type t) { return t == TYPE_FLOAT || t == TYPE_DOUBLE; }

static Type promote(Type a, Type b) {
    if (a == TYPE_DOUBLE || b == TYPE_DOUBLE) return TYPE_DOUBLE;
    if (a == TYPE_FLOAT  || b == TYPE_FLOAT)  return TYPE_FLOAT;
    return TYPE_INT;
}

static ConstVal mk_int(long v, Type t) {
    ConstVal c = { t, v, 0.0 };
    return c;
}

static ConstVal mk_real(double v, Type t) {
    ConstVal c = { t, 0, v };
    return c;
}

static double as_real(ConstVal c) {
    return is_real(c.type) ? c.d : (double)c.i;
}

long char_literal_value(const char *text) {
    if (!text || !text[0]) return 0;
    if (text[0] != '\\' || text[1] == '\0') return (unsigned char)text[0];
    switch (text[1]) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case 'b':  return '\b';
    case 'f':  return '\f';
    case '0':  return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"':  return '"';
    default:   return (unsigned char)text[1];
    }
}

ConstVal const_convert(ConstVal c, Type t) {
    if (is_real(t)) {
        double d = as_real(c);
        if (t == TYPE_FLOAT) d = (float)d;
        return mk_real(d, t);
    }
    if (t == TYPE_BOOLEAN || c.type == TYPE_BOOLEAN)
        return mk_int(c.i ? 1 : 0, t);
    long v = is_real(c.type) ? (long)c.d : c.i;
    if (t == TYPE_CHAR) return mk_int((uint16_t)v, TYPE_CHAR);
    return mk_int((int32_t)v, TYPE_INT);
}

static int is_relational(OpKind op) {
    return op == OP_LT || op == OP_GT || op == OP_LE || op == OP_GE ||
           op == OP_EQ || op == OP_NE;
}

int const_eval(Node *n, ConstVal *out) {
    if (!n) return 0;
    switch (n->kind) {
    case NODE_INT:
        *out = mk_int((int32_t)n->val.i, TYPE_INT);
        return 1;
    case NODE_FLOAT:
        *out = const_convert(mk_real(n->val.d, TYPE_DOUBLE),
                             n->decl_type == TYPE_FLOAT ? TYPE_FLOAT : TYPE_DOUBLE);
        return 1;
    case NODE_CHAR:
        *out = mk_int(char_literal_value(n->name), TYPE_CHAR);
        return 1;
    case NODE_BOOL:
        *out = mk_int(n->val.i ? 1 : 0, TYPE_BOOLEAN);
        return 1;

    case NODE_CAST: {
        ConstVal a;
        if (!const_eval(n->a, &a)) return 0;
        *out = const_convert(a, n->decl_type);
        return 1;
    }

    case NODE_UNARY: {
        ConstVal a;
        if (!const_eval(n->a, &a)) return 0;
        if (n->op == OP_NOT)       *out = mk_int(!a.i, TYPE_BOOLEAN);
        else if (is_real(a.type))  *out = mk_real(-a.d, a.type);
        else                       *out = const_convert(mk_int(-a.i, TYPE_INT), TYPE_INT);
        return 1;
    }

    case NODE_BINOP: {
        ConstVal l, r;
        if (!const_eval(n->a, &l) || !const_eval(n->b, &r)) return 0;
        if (n->op == OP_AND || n->op == OP_OR) {
            int v = (n->op == OP_AND) ? (l.i && r.i) : (l.i || r.i);
            *out = mk_int(v, TYPE_BOOLEAN);
            return 1;
        }
        if (is_relational(n->op)) {
            int v;
            if (is_real(l.type) || is_real(r.type)) {
                double a = as_real(l), b = as_real(r);
                switch (n->op) {
                case OP_LT: v = a <  b; break;
                case OP_GT: v = a >  b; break;
                case OP_LE: v = a <= b; break;
                case OP_GE: v = a >= b; break;
                case OP_EQ: v = a == b; break;
                default:    v = a != b; break;
                }
            } else {
                long a = l.i, b = r.i;
                switch (n->op) {
                case OP_LT: v = a <  b; break;
                case OP_GT: v = a >  b; break;
                case OP_LE: v = a <= b; break;
                case OP_GE: v = a >= b; break;
                case OP_EQ: v = a == b; break;
                default:    v = a != b; break;
                }
            }
            *out = mk_int(v, TYPE_BOOLEAN);
            return 1;
        }
        Type t = promote(l.type, r.type);
        if (is_real(t)) {
            double a = as_real(l), b = as_real(r), v;
            switch (n->op) {
            case OP_ADD:  v = a + b; break;
            case OP_SUB:  v = a - b; break;
            case OP_MULT: v = a * b; break;
            case OP_DIV:  v = a / b; break;
            default: return 0; /* % de reais não é válido no subconjunto */
            }
            *out = const_convert(mk_real(v, TYPE_DOUBLE), t);
            return 1;
        }
        /* inteiros: calcula em 64 bits e reduz a 32 (wrap-around do Java) */
        long a = (int32_t)l.i, b = (int32_t)r.i, v;
        switch (n->op) {
        case OP_ADD:  v = a + b; break;
        case OP_SUB:  v = a - b; break;
        case OP_MULT: v = a * b; break;
        case OP_DIV:  if (b == 0) return 0; v = a / b; break;
        case OP_MOD:  if (b == 0) return 0; v = a % b; break;
        default: return 0;
        }
        *out = const_convert(mk_int(v, TYPE_INT), TYPE_INT);
        return 1;
    }

    default:
        return 0;
    }
}

int const_bool(Node *n, int *value) {
    ConstVal c;
    if (!const_eval(n, &c) || c.type != TYPE_BOOLEAN) return 0;
    *value = c.i ? 1 : 0;
    return 1;
}
