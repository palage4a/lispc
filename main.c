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

typedef struct {
  int type;
  union lval_union {
    double num;
    int err;
  } val;
} lval;

enum { LVAL_NUMM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval lval_num(double x) {
  lval v;
  v.type = LVAL_NUMM;
  v.val.num = x;
  return v;
}

lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.val.err = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
  case LVAL_NUMM: printf("%g\n", v.val.num); break;
  case LVAL_ERR:
    switch (v.val.err) {
    case LERR_BAD_OP:
      puts("Error: Invalid operator");
      break;
    case LERR_DIV_ZERO:
      puts("Error: Zero division");
      break;
    case LERR_BAD_NUM:
      puts("Error: Invalid number");
      break;
    }
    break;
  }
}

lval eval_op(lval x, char* op, lval y) {
  if (x.type == LVAL_ERR) return x;
  if (y.type == LVAL_ERR) return y;

  if (strcmp(op, "+") == 0) return lval_num(x.val.num + y.val.num);
  if (strcmp(op, "-") == 0) return lval_num(x.val.num - y.val.num);
  if (strcmp(op, "*") == 0) return lval_num(x.val.num * y.val.num);
  if (strcmp(op, "%") == 0) return lval_num(remainder(x.val.num, y.val.num));
  if (strcmp(op, "/") == 0) {
    if (y.val.num == 0) return lval_err(LERR_DIV_ZERO);
    return lval_num(x.val.num / y.val.num);
  }
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  char* op = t->children[1]->contents;

  lval x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }
  return x;
}

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
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT, "\
number: /-?[0-9]+/;                             \
operator: '+' | '-' | '*' | '/' | '%';          \
expr: <number> | '(' <operator> <expr> + ')';   \
lispy: /^/ <operator> <expr>+ /$/ ;            \
", Number, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit");

  while (1) {
    /* debug(); */
    char *input = readline("lispc> ");
    add_history(input);

    // NOTE: mpc_result_t* does not work event we use -> later. why?
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r )) {
      lval res = eval(r.output);
      lval_print(res);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}

