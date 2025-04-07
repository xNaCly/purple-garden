#include "builtins.h"
#include "common.h"

builtin_function BUILTIN_MAP[] = {
    [BUILTIN_PRINTLN] = &builtin_println,
    [BUILTIN_PRINT] = &builtin_print,
    [BUILTIN_LEN] = &builtin_len,
};

Str BUILTIN_NAME_MAP[] = {
    [BUILTIN_PRINTLN] = STRING("println"),
    [BUILTIN_PRINT] = STRING("print"),
    [BUILTIN_LEN] = STRING("len"),
};

static void print_value(const Value v) {
  switch (v.type) {
  case V_OPTION: {
    if (v.option.is_some) {
      printf("Some(");
      print_value(*v.option.value);
      putc(')', stdout);
    } else {
      printf("None");
    }
    break;
  }
  case V_STRING:
    Str_debug(&v.string);
    break;
  case V_NUM:
    printf("%f", v.number);
    break;
  case V_TRUE:
    printf("true");
    break;
  case V_FALSE:
    printf("false");
    break;
  case V_LIST:
    // TODO: iterate each one and print with ,
  case V_UNDEFINED:
    printf("undefined");
    break;
  default:
  }
}

Value builtin_println(const Value arg) {
  print_value(arg);
  putc('\n', stdout);
  return NONE;
}

Value builtin_print(const Value arg) {
  print_value(arg);
  return NONE;
}

Value builtin_len(const Value arg) {
  if (arg.type == V_STRING) {
    return (Value){.type = V_NUM, .number = arg.string.len};
  } else if (arg.type == V_LIST) {
    TODO("builtin_len#arg->type == V_LIST not implemented")
  } else {
    fputs("builtin_len only strings and lists have a length", stderr);
    // TODO: think about failing here
    exit(EXIT_FAILURE);
  }
  return NONE;
}
