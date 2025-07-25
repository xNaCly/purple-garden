#include "../cc.h"
#include "../common.h"
#include "../lexer.h"
#include "../parser.h"
#include "../vm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  Str input;
  Value expected_r0;
} Case;

#define PRINT_DISASM_ON_ERR 0

#define VAL(...)                                                               \
  (Value) { __VA_ARGS__ }

#define CASE(in, r0)                                                           \
  { .input = STRING(#in "\0"), .expected_r0 = r0, }

// stolen from common.(c|h) and adapted
bool Value_cmp_deep(const Value *a, const Value *b) {
  // fastpath if value pointers are equal
  if (a == b) {
    return true;
  }

  if (a->type != b->type) {
    return false;
  }

  if (a->is_some ^ b->is_some) {
    return false;
  }

  switch (a->type) {
  case V_STR:
    return Str_eq(&a->string, &b->string);
  case V_DOUBLE:
    double diff = a->floating - b->floating;
#define PREC 1e-9
    return (diff < PREC && diff > -PREC);
  case V_INT:
    return a->integer == b->integer;
  case V_TRUE:
  case V_FALSE:
  case V_NONE:
    return true;
  case V_ARRAY:
    // TODO: implement deep array comparison
  default:
    // lists arent really the same, this is not a deep equal
    return false;
  }
}

int main() {
  Case cases[] = {
    // atoms:

    // doubles
    CASE(3.1415, VAL(.type = V_DOUBLE, .floating = 3.1415)),
    CASE(.1415, VAL(.type = V_DOUBLE, .floating = 0.1415)),

    CASE("string", VAL(.type = V_STR, .string = STRING("string"))),
    {
        .input = STRING("'this-is-really-a-quoted-symbol"),
        .expected_r0 =
            (Value){.type = V_STR,
                    .string = STRING("this-is-really-a-quoted-symbol")},
    },
    // TODO: this is for future me to implement
    // CASE("escaped string\"", BC(OP_LOAD, 0), VAL(.type = V_STRING, .string
    // = STRING("escaped string\""))),
    CASE(true false, VAL(.type = V_FALSE)),
    // checking if boolean interning works
    CASE(true false true false, VAL(.type = V_FALSE)),
    CASE("hello", VAL(.type = V_STR, .string = STRING("hello"))),

    // too large integer and double values
    // https://github.com/xNaCly/purple-garden/issues/1
    // CASE(9223372036854775807, VAL(.type = V_UNDEFINED)),
    // CASE(
    //     179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858369.0,
    //     VAL(.type = V_UNDEFINED)),

    // math:
    CASE((+2 2), VAL(.type = V_INT, .integer = 4)),
    CASE((-5 3), VAL(.type = V_INT, .integer = 2)),
    CASE((*3 4), VAL(.type = V_INT, .integer = 12)),
    CASE((/ 6 2), VAL(.type = V_INT, .integer = 3)),
    CASE((+1(-2 1)), VAL(.type = V_INT, .integer = 2)),

    // double and int math:
    CASE((+2.0 2), VAL(.type = V_DOUBLE, .floating = 4.0)),
    CASE((+2 2.0), VAL(.type = V_DOUBLE, .floating = 4.0)),
    CASE((-5.0 3), VAL(.type = V_DOUBLE, .floating = 2)),
    CASE((-5 3.0), VAL(.type = V_DOUBLE, .floating = 2)),
    CASE((*3.0 4), VAL(.type = V_DOUBLE, .floating = 12)),
    CASE((*3 4.0), VAL(.type = V_DOUBLE, .floating = 12)),
    CASE((/ 6.0 2), VAL(.type = V_DOUBLE, .floating = 3)),
    CASE((/ 6 2.0), VAL(.type = V_DOUBLE, .floating = 3)),

    CASE((@len "hello"), VAL(.type = V_INT, .integer = 5)),
    // checking if string interning works
    CASE((@len "hello")(@len "hello"), VAL(.type = V_INT, .integer = 5)),
    CASE((@len ""), VAL(.type = V_INT, .integer = 0)),
    CASE((@len "a"), VAL(.type = V_INT, .integer = 1)),

    CASE((= 1 1), VAL(.type = V_TRUE)),
    CASE((= "abc"
            "abc"),
         VAL(.type = V_TRUE)),
    CASE((= 3.1415 3.1415), VAL(.type = V_TRUE)),
    CASE((= true true), VAL(.type = V_TRUE)),
    CASE((= true false), VAL(.type = V_FALSE)),
    CASE((= false false), VAL(.type = V_TRUE)),

    // variables
    CASE((@let name "user")name, VAL(.type = V_STR, .string = STRING("user"))),
    CASE((@let age 25)age, VAL(.type = V_INT, .integer = 25)),

    // functions
    CASE((@function ret[arg] arg)(ret 25), VAL(.type = V_INT, .integer = 25)),
    CASE((@function add25[arg](+arg 25))(add25 25),
         VAL(.type = V_INT, .integer = 50)),

    // builtins
    CASE((@assert true), VAL(.type = V_TRUE)),
    CASE((@None), VAL(.type = V_NONE)),
    CASE((@Some true), VAL(.type = V_TRUE, .is_some = true)),
    CASE((@Some false), VAL(.type = V_FALSE, .is_some = true)),

    // match
    //
    // default
    CASE((@match true), VAL(.type = V_TRUE)),
    CASE((@match false), VAL(.type = V_FALSE)),
  };

  size_t passed = 0;
  size_t failed = 0;
  size_t len = sizeof(cases) / sizeof(Case);
  for (size_t i = 0; i < len; i++) {
    Case c = cases[i];
    size_t min_size = (
                          // size for globals
                          (MIN_MEM * sizeof(Value))
                          // size for bytecode
                          + MIN_MEM
                          // size for nodes
                          + (MIN_MEM * sizeof(Node))) *
                      2;

    Lexer l = Lexer_new(c.input);
    Allocator *alloc = bump_init(min_size);
    // specifically omitted, since the tests should not flake or have any memory
    // issues that could be the result of gc issues, thus the bump allocator
    // from bump.c is used and Vm_destroy calls alloc->destroy on said allocator
    //
    // CALL(alloc, destroy);
    Token **tokens = CALL(alloc, request, MIN_MEM * sizeof(Token));
    Lexer_all(&l, alloc, tokens);
    Parser p = Parser_new(alloc, tokens);
    size_t node_count = MIN_MEM * sizeof(Node *) / 4;
    Node **nodes = CALL(alloc, request, node_count);
    node_count = Parser_all(nodes, &p, node_count);
    // tests only use the bump allocator, no gc, as explained above, thus both
    // static and vm runtime allocator are simply: alloc
    Vm _vm = Vm_new(alloc, alloc);
    Vm *vm = &_vm;
    Ctx ctx = cc(vm, alloc, nodes, node_count);

    bool error = false;
    Vm_run(vm);
    if (!Value_cmp_deep(&vm->registers[0], &c.expected_r0)) {
      printf("\tbad value at r0");
      printf("\n\twant=");
      Value_debug(&c.expected_r0);
      printf("\n\tgot=");
      Value_debug(&vm->registers[0]);
      puts("");
#if PRINT_DISASM_ON_ERR
      disassemble(vm, &ctx);
#endif
      error = true;
    }

    if (error) {
      failed++;
      printf("[-][FAIL][Case %zu/%zu] in=`%s` \n", i + 1, len, c.input.p);
    } else {
      passed++;
      printf("[+][PASS][Case %zu/%zu] in=`%s` \n", i + 1, len, c.input.p);
    }
    Vm_destroy(vm);
  }

  printf("[=] %zu/%zu passed, %zu failed\n", passed, len, failed);

  return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
