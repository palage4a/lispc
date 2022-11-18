#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mpc/mpc.h"

// TODO: check that code
#ifdef _WIN32
static char buffer[2048]

char* readline(char *prompt) {
  fputs(prompt, stdin);
  fgets(buffer, 2048, stdin);
  return buffer;
}
#else
#include <editline/readline.h>
#endif

typedef struct lval {
  int type;
  union lval_union {
    double num;
    char* sym;
    char* err;
  } val;
  int count;
  struct lval** cell;
} lval;

enum { LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_ERR };

void lval_print(lval* v);
lval* lval_eval(lval* v);
lval* lval_eval_sexpr(lval* v);
lval* lval_nnpop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* builtin_op(lval* v, char* s);

lval* lval_num(double x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->val.num = x;
  return v;
}

lval* lval_err(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->val.err = malloc(strlen(s) + 1);
  strcpy(v->val.err, s);
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->val.sym = malloc(strlen(s) + 1);
  strcpy(v->val.sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval* v) {
  switch(v->type) {
  case LVAL_NUM: break;
  case LVAL_ERR: free(v->val.err); break;
  case LVAL_SYM: free(v->val.sym); break;
  case LVAL_SEXPR:
    for(int i = 0; i < v->count; i++) {
      free(v->cell[i]);
    }
    free(v->cell);
    break;
  }
  free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval* lval_add(lval* x, lval* y) {
  x->count++;
  x->cell = realloc(x->cell, sizeof(lval*) * x->count);
  x->cell[x->count-1] = y;
  return x;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i+1],
          sizeof(lval*) * (v->count-i-1));
  v->count--;

  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_op(lval* v, char* op) {
  for(int i = 0; i < v->count; i++) {
    if (v->cell[i]->type != LVAL_NUM) {
      lval_del(v);
      return lval_err("Can not operates non-numbers");
    }
  }

  lval* x = lval_pop(v, 0);

  if ((strcmp(op, "-") == 0) && (v->count == 0)) {
    x->val.num = -x->val.num;
  }

  while (v->count != 0) {
    lval* y = lval_pop(v, 0);

    if (strcmp(op, "+") == 0) x->val.num += y->val.num;
    if (strcmp(op, "-") == 0) x->val.num -= y->val.num;
    if (strcmp(op, "*") == 0) x->val.num *= y->val.num;
    if (strcmp(op, "/") == 0) {
      if (y->val.num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero");
        break;
      }
      x->val.num /= y->val.num;
    }
  }

  lval_del(v);
  return x;
}

lval* lval_eval(lval* v) {
  return v->type == LVAL_SEXPR ? lval_eval_sexpr(v) : v;
}

lval* lval_eval_sexpr(lval* v) {
  for (int i = 0; i < v->count; i++ ){
    v->cell[i] = lval_eval(v->cell[i]);
  }

  for (int i = 0; i< v->count; i++ ) {
    if (v->type == LVAL_ERR) {
      return lval_take(v->cell[i], i);
    }
  }

  if (v->count == 0) return v;

  if (v->count == 1) return lval_take(v, 0);

  lval* f = lval_pop(v, 0);
  if(f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-Expresions does not start with operator");
  }

  lval* result = builtin_op(v, f->val.sym);
  lval_del(f);

  return result;
}

lval* lval_read(mpc_ast_t* t) {
  /* printf("%s\n", t->tag); */
  /* printf("%s\n", t->contents); */

  if (strstr(t->tag, "number")) return lval_read_num(t);
  if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
  if (strstr(t->tag, "sexpr")) x = lval_sexpr();
  x = lval_sexpr(); // more secure way

  for(int i = 0; i < t->children_num; i ++ ) {
    if (strcmp(t->children[i]->contents, "(") == 0) continue;
    if (strcmp(t->children[i]->contents, ")") == 0) continue;
    if (strcmp(t->children[i]->tag, "regex") == 0)  continue;
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}


void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for(int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i != (v->count-1)) putchar(' ');
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
  case LVAL_ERR: printf("ERR: %s", v->val.err); break;
  case LVAL_SYM: printf("%s", v->val.sym); break;
  case LVAL_NUM: printf("%g", v->val.num); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
  return;
}

void lval_println(lval* v) { lval_print(v); putchar('\n');}

void debug() {
  double a = 0;
  double b = 3;
  int z = 0;

  printf("%f\n", a + b); // auto convert integer number due to   \
                         // type of variable e.g. to decimal
  printf("%d\n", a == z); // double zero equals integer zero
  errno = 0;
  char* end;
  printf("%f\n", strtof(".32", &end));
  printf("%d\n", errno);
  printf("%p\n", end);
}

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT, "\
number: /-?[0-9]+/;                             \
symbol: '+' | '-' | '*' | '/' | '%';            \
sexpr: '(' <expr>* ')';                       \
expr: <number> | <symbol> | <sexpr>;           \
lispy: /^/ <expr>* /$/ ;            \
", Number, Symbol, Sexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit");

  while (1) {
    /* debug(); */
    char *input = readline("lispc> ");
    add_history(input);

    // NOTE: mpc_result_t* does not work event we use -> later. why?
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r )) {
      lval* res = lval_eval(lval_read(r.output));
      lval_println(res);
      lval_del(res);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }

  mpc_cleanup(4, Number, Symbol, Sexpr, Expr, Lispy);

  return 0;
}

