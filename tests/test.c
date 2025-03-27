#include "../cc.h"
#include "../common.h"
#include "../lexer.h"
#include "../parser.h"
#include "../vm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  String input;
  size_t expected_size;
  byte *expected;
  Value expected_r0;
} Case;

#define BC(...)                                                                \
  (byte[]) { __VA_ARGS__ }
#define CASE(in, ex, r0)                                                       \
  {                                                                            \
    .input = STRING(in), .expected = ex,                                       \
    .expected_size = sizeof(ex) / sizeof(byte), .expected_r0 = (Value)r0,      \
  }

int main() {
  Case cases[] = {
      {
          .input = ((String){.len = sizeof("3.1415"), .p = "3.1415"}),
          .expected = (byte[]){OP_LOAD, 0},
          .expected_size = sizeof((byte[]){OP_LOAD, 0}) / sizeof(byte),
          .expected_r0 = (Value){.type = V_NUM, .number = 3.1415},
      },
      {
          .input = ((String){.len = sizeof("\"string\""), .p = "\"string\""}),
          .expected = (byte[]){OP_LOAD, 0},
          .expected_size = sizeof((byte[]){OP_LOAD, 0}) / sizeof(byte),
          .expected_r0 = (Value){.type = V_STRING, .string = STRING("string")},
      },
      CASE("true false", BC(OP_LOAD, 0, OP_LOAD, 1), (Value){.type = V_FALSE}),
      {
          .input = ((String){.len = sizeof("ident"), .p = "ident"}),
          .expected = (byte[]){OP_LOAD, 0, OP_VAR, 0},
          .expected_size =
              sizeof((byte[]){OP_LOAD, 0, OP_VAR, 0}) / sizeof(byte),
          .expected_r0 = (Value){.type = V_STRING, .string = STRING("ident")},
      },
  };
  size_t passed = 0;
  size_t failed = 0;
  size_t len = sizeof(cases) / sizeof(Case);
  for (size_t i = 0; i < len; i++) {
    Case c = cases[i];
    puts("================= CASE =================");
    printf("[Case %zu/%zu] '%s' \n", i + 1, len, c.input.p);
    Lexer l = Lexer_new(c.input);
    Parser p = Parser_new(&l);
    Node ast = Parser_run(&p);
#if DEBUG
    puts("----------------- AST ------------------");
    Node_debug(&ast, 0);
    puts("");
#endif
    Vm raw = cc(&ast);
    Vm *vm = &raw;

    bool error = false;
    if (c.expected_size != vm->bytecode_len) {
      printf("\tlenght not equal: wanted %zu, got %zu\n", c.expected_size,
             vm->bytecode_len);
      error = true;
    } else {
      for (size_t j = 0; j < vm->bytecode_len; j += 2) {
        size_t expected_op = c.expected[j];
        size_t got_op = vm->bytecode[j];

        size_t expected_arg = c.expected[j + 1];
        size_t got_arg = vm->bytecode[j + 1];

        if (expected_op != got_op) {
#if DEBUG
          printf("\tbad operator: want=%s got=%s\n", OP_MAP[expected_op].p,
                 OP_MAP[got_op].p);
#else
          printf("\n\tbad operator: want=%zu got=%zu\n", expected_op, got_op);
#endif

          error = true;
        }
        if (expected_arg != got_arg) {
          printf("\tbad arg: want=%zu got=%zu\n", expected_arg, got_arg);
          error = true;
        }
      }
    }
#if DEBUG
    puts("----------------- CODE -----------------");
#endif
    Vm_run(vm);
    if (!Vm_Value_cmp(vm->_registers[0], c.expected_r0)) {
#if DEBUG
      printf("\n\tbad value at r0: want=%s got=%s\n",
             VALUE_MAP[c.expected_r0.type].p,
             VALUE_MAP[vm->_registers[0].type].p);
#else
      printf("\n\tbad value at r0: want=%d got=%d\n", c.expected_r0.type,
             vm->_registers[0].type);
#endif
    }
    Node_destroy(&ast);

    if (error) {
      failed++;
      puts("~> failed!");
    } else {
      passed++;
      puts("~> passed!");
    }
    Vm_destroy(raw);
  }

  printf("%zu of %zu passed, %zu failed\n", passed, len, failed);

  return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
