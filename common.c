#include "common.h"
#include <string.h>

#define PREC 1e-9

Str VALUE_TYPE_MAP[] = {
    [V_NONE] = STRING("Option::None"), [V_STR] = STRING("Str"),
    [V_INT] = STRING("Int"),           [V_DOUBLE] = STRING("Double"),
    [V_TRUE] = STRING("True"),         [V_FALSE] = STRING("False"),
    [V_ARRAY] = STRING("Array"),
};

// Value_cmp compares two values in a shallow way, is used for OP_EQ and in
// tests.
//
// Edgecases:
// - V_LIST & V_LIST is false, because we do a shallow compare
// - V_OPTION(Some(A)) & V_OPTION(Some(B)) even with matching A and B is false,
// since we do not compare inner
inline bool Value_cmp(const Value *a, const Value *b) {
  // fastpath if value pointers are equal
  if (a == b) {
    return true;
  }

  if (a->type != b->type) {
    return false;
  }

  // any is an optional, we dont compare deeply
  if (a->is_some || b->is_some) {
    return false;
  }

  switch (a->type) {
  case V_STR:
    return Str_eq(&a->string, &b->string);
  case V_DOUBLE:
    // PERF: can potentially be faster, since we omit need a function call, in
    // practice i havent seen any impact over the following construct. if
    //
    // (memcmp(&a->floating, &b->floating, sizeof(double)) == 0)
    //   return true;

    double diff = a->floating - b->floating;
    return (diff < PREC && diff > -PREC);
  case V_INT:
    return a->integer == b->integer;
  case V_TRUE:
  case V_FALSE:
  case V_NONE:
    return true;
  case V_ARRAY:
  default:
    // lists arent really the same, this is not a deep equal
    return false;
  }
}

void Value_debug(const Value *v) {
  Str *t = &VALUE_TYPE_MAP[v->type];
  if (t != NULL) {
    if (v->is_some) {
      printf("Option::Some(");
    }
    Str_debug(t);
    if (v->is_some) {
      printf(")");
    }
  }
  switch (v->type) {
  case V_NONE:
  case V_TRUE:
  case V_FALSE:
    break;
  case V_STR:
    printf("(`");
    Str_debug(&v->string);
    printf("`)");
    break;
  case V_DOUBLE:
    printf("(%g)", v->floating);
    break;
  case V_INT:
    printf("(%ld)", v->integer);
    break;
  case V_ARRAY: {
    printf("[");
    for (size_t i = 0; i < v->array.len; i++) {
      Value_debug(v->array.value[i]);
    }
    printf("]");
    break;
  };
  default:
    printf("<unkown>");
  }
}

inline double Value_as_double(const Value *v) {
  if (v->type == V_DOUBLE) {
    return v->floating;
  } else if (v->type == V_INT) {
    return (double)v->integer;
  } else {
    ASSERT(0, "Value is neither double nor int, cant convert to double")
  }
}

#undef PREC
