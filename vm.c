#include "vm.h"
#include "common.h"
#include "mem.h"
#include "strings.h"
#include <stdint.h>

static builtin_function BUILTIN_MAP[MAX_BUILTIN_SIZE];

inline void Vm_register_builtin(Vm *vm, builtin_function bf, Str name) {
  vm->builtins[Str_hash(&name) & MAX_BUILTIN_SIZE_MASK] = bf;
}

Vm Vm_new(Allocator *alloc) {
  Vm vm = {
      .alloc = alloc,
      .global_len = 0,
      .bytecode_len = 0,
      .pc = 0,
      .bytecode = NULL,
      .globals = NULL,
  };

  vm.builtins = (void **)BUILTIN_MAP;
  vm.bytecode = alloc->request(alloc->ctx, (sizeof(uint32_t) * BYTECODE_SIZE));
  vm.globals = alloc->request(alloc->ctx, (sizeof(Value *) * GLOBAL_SIZE));
  vm.globals[0] = INTERNED_FALSE;
  vm.globals[1] = INTERNED_TRUE;
  vm.globals[2] = INTERNED_NONE;
  vm.global_len = 3;

  Vm_register_builtin(&vm, builtin_print, STRING("print"));
  Vm_register_builtin(&vm, builtin_println, STRING("println"));
  Vm_register_builtin(&vm, builtin_len, STRING("len"));
  Vm_register_builtin(&vm, builtin_type, STRING("type"));
  Vm_register_builtin(&vm, builtin_Some, STRING("Some"));
  // Vm_register_builtin(&vm, builtin_None, STRING("None"));

  return vm;
}

Str OP_MAP[256] = {
    [OP_LOADG] = STRING("LOADG"), [OP_LOAD] = STRING("LOAD"),
    [OP_STORE] = STRING("STORE"), [OP_ADD] = STRING("ADD"),
    [OP_SUB] = STRING("SUB"),     [OP_MUL] = STRING("MUL"),
    [OP_DIV] = STRING("DIV"),     [OP_EQ] = STRING("EQ"),
    [OP_VAR] = STRING("VAR"),     [OP_LOADV] = STRING("LOADV"),
    [OP_ARGS] = STRING("ARGS"),   [OP_BUILTIN] = STRING("BUILTIN"),
    [OP_LEAVE] = STRING("LEAVE"), [OP_CALL] = STRING("CALL"),
    [OP_JMP] = STRING("JMP"),     [OP_ASSERT] = STRING("ASSERT"),
};

// FrameFreeList works by caching a list of Frames so we don't have to interact
// with the heap for the first N Frames, but instead use the preallocated
// frames. On entering a scope, pop a frame and use it, on leaving a scope,
// clear the frame and return it.
typedef struct {
  Frame *head;
  Allocator *alloc;
} FrameFreeList;

void freelist_preallocate(FrameFreeList *fl) {
  // PERF: maybe 256 is too many, but prefetching a recursion depth can have
  // some positive effects on the runtime performance
  for (size_t i = 0; i < 256; i++) {
    Frame *frame = fl->alloc->request(fl->alloc->ctx, sizeof(Frame));
    frame->prev = fl->head;
    fl->head = frame;
  }
}

#if DEBUG
static size_t frame_count = 1;
#endif

void freelist_push(FrameFreeList *fl, Frame *frame) {
  frame->prev = fl->head;
  // memset(frame->variable_table, 0, sizeof(frame->variable_table));
  frame->return_to_bytecode = 0;
  fl->head = frame;
}

#define NEW(...)                                                               \
  ({                                                                           \
    Value *__v = vm->alloc->request(vm->alloc->ctx, sizeof(Value));            \
    *__v = (Value)__VA_ARGS__;                                                 \
    __v;                                                                       \
  })

Frame *freelist_pop(FrameFreeList *fl) {
  if (!fl->head)
    return fl->alloc->request(fl->alloc->ctx, sizeof(Frame));
  Frame *f = fl->head;
  fl->head = f->prev;
  f->prev = NULL;
  return f;
}

int Vm_run(Vm *vm) {
  FrameFreeList *fl = &(FrameFreeList){.alloc = vm->alloc};
  freelist_preallocate(fl);
  vm->arg_count = 1;
  vm->frame = freelist_pop(fl);
  while (vm->pc < vm->bytecode_len) {
    VM_OP op = vm->bytecode[vm->pc];
    uint32_t arg = vm->bytecode[vm->pc + 1];

#if DEBUG
    vm->instruction_counter[op]++;
#endif

    switch (op) {
    case OP_LOADG:
      vm->registers[0] = *vm->globals[arg];
      break;
    case OP_LOAD:
      vm->registers[0] = vm->registers[arg];
      break;
    case OP_LOADV: {
      // bounds checking and checking for variable validity is performed at
      // compile time, but we still have to check if the variable is available
      // in the current scope...
      Value *v = vm->frame->variable_table[arg];
      if (v == NULL) {
        VM_ERR("Undefined variable with hash %i", arg);
      }
      vm->registers[0] = *v;
      break;
    }
    case OP_STORE:
      vm->registers[arg] = vm->registers[0];
      break;
    case OP_VAR:
      vm->frame->variable_table[arg] = NEW(vm->registers[0]);
      break;
    case OP_ADD: {
      Value *left = &vm->registers[0];
      Value *right = &vm->registers[arg];
      if (left->type == V_DOUBLE || right->type == V_DOUBLE) {
        vm->registers[0].floating =
            Value_as_double(right) + Value_as_double(left);
        vm->registers[0].type = V_DOUBLE;
      } else {
        if (!(left->type == V_INT && right->type == V_INT)) {
          VM_ERR("VM[+] Incompatible types %.*s and %.*s",
                 (int)VALUE_TYPE_MAP[left->type].len,
                 VALUE_TYPE_MAP[left->type].p,
                 (int)VALUE_TYPE_MAP[right->type].len,
                 VALUE_TYPE_MAP[right->type].p)
        }
        vm->registers[0].integer = right->integer + left->integer;
        vm->registers[0].type = V_INT;
      }
      break;
    }
    case OP_SUB: {
      Value *left = &vm->registers[0];
      Value *right = &vm->registers[arg];
      if (left->type == V_DOUBLE || right->type == V_DOUBLE) {
        vm->registers[0].floating =
            Value_as_double(right) - Value_as_double(left);
        vm->registers[0].type = V_DOUBLE;
      } else {
        if (!(left->type == V_INT && right->type == V_INT)) {
          VM_ERR("VM[-] Incompatible types %.*s and %.*s",
                 (int)VALUE_TYPE_MAP[left->type].len,
                 VALUE_TYPE_MAP[left->type].p,
                 (int)VALUE_TYPE_MAP[right->type].len,
                 VALUE_TYPE_MAP[right->type].p)
        }
        vm->registers[0].type = V_INT;
        vm->registers[0].integer = right->integer - left->integer;
      }
      break;
    }
    case OP_MUL: {
      Value *left = &vm->registers[0];
      Value *right = &vm->registers[arg];
      if (left->type == V_DOUBLE || right->type == V_DOUBLE) {
        vm->registers[0].floating =
            Value_as_double(right) * Value_as_double(left);
        vm->registers[0].type = V_DOUBLE;
      } else {
        if (!(left->type == V_INT && right->type == V_INT)) {
          VM_ERR("VM[*] Incompatible types %.*s and %.*s",
                 (int)VALUE_TYPE_MAP[left->type].len,
                 VALUE_TYPE_MAP[left->type].p,
                 (int)VALUE_TYPE_MAP[right->type].len,
                 VALUE_TYPE_MAP[right->type].p)
        }
        vm->registers[0].type = V_INT;
        vm->registers[0].integer = right->integer * left->integer;
      }
      break;
    }
    case OP_DIV: {
      Value *left = &vm->registers[0];
      Value *right = &vm->registers[arg];
      if (left->type == V_DOUBLE || right->type == V_DOUBLE) {
        vm->registers[0].floating =
            Value_as_double(right) / Value_as_double(left);
        vm->registers[0].type = V_DOUBLE;
      } else {
        if (!(left->type == V_INT && right->type == V_INT)) {
          VM_ERR("VM[/] Incompatible types %.*s and %.*s",
                 (int)VALUE_TYPE_MAP[left->type].len,
                 VALUE_TYPE_MAP[left->type].p,
                 (int)VALUE_TYPE_MAP[right->type].len,
                 VALUE_TYPE_MAP[right->type].p)
        }
        vm->registers[0].type = V_INT;
        vm->registers[0].integer = right->integer / left->integer;
      }
      break;
    }
    case OP_EQ: {
      // pointer comparison fast path
      vm->registers[0] = Value_cmp(&vm->registers[0], &vm->registers[arg])
                             ? *vm->globals[1]
                             : *vm->globals[0];
      break;
    }
    case OP_ARGS:
      vm->arg_count = arg;
      break;
    case OP_BUILTIN: {
      // at this point all builtins are just syscalls into an array of
      // function pointers
      ((builtin_function)vm->builtins[arg])(vm);
      vm->arg_count = 1;
      break;
    }
    case OP_CALL: {
      Frame *f = freelist_pop(fl);
      f->prev = vm->frame;
      f->return_to_bytecode = vm->pc;
      vm->frame = f;
      vm->pc = arg;
      vm->arg_count = 1;
      break;
    }
    case OP_LEAVE: {
      Frame *old = vm->frame;
      if (vm->frame->prev) {
        vm->pc = vm->frame->return_to_bytecode;
        vm->frame = vm->frame->prev;
      }
      freelist_push(fl, old);
      break;
    }
    case OP_JMP: {
      vm->pc = arg;
      break;
    }
    case OP_ASSERT: {
      if (vm->registers[0].type != V_TRUE) {
        VM_ERR("Assertion failed, value is not true")
      }
      break;
    }
    default:
      VM_ERR("Unimplemented instruction %.*s", (int)OP_MAP[op].len,
             OP_MAP[op].p)
    }
    vm->pc += 2;
  }
  return 0;
vm_end:
  return 1;
}

void Vm_destroy(Vm *vm) {}
