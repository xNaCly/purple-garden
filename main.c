#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "cc.h"
#include "common.h"
#include "io.h"
#include "lexer.h"
#include "mem.h"
#include "parser.h"
#include "vm.h"

#define CTX "pre"
#define VERSION "alpha"
#ifndef COMMIT
#define COMMIT "(no commit)"
#endif

#ifndef BENCH
#define BENCH 0
#endif

typedef struct {
  // options - int because getopt has no bool support

  // use block allocator instead of garbage collection
  int block_allocator;
  // compile all functions to machine code
  int aot_functions;
  // readable bytecode representation with labels, globals and comments
  int disassemble;
  // display the memory usage of parsing, compilation and the virtual machine
  int memory_usage;

  // options in which we exit after toggle
  int version;
  int help;

  // entry point - last argument thats not an option
  char *filename;
} Args;

typedef struct {
  char *name_long;
  char name_short;
  char *description;
} cli_option;

// WARN: DO NOT REORDER THIS - will result in option handling issues
static const cli_option options[] = {
    {"version", 'v', "display version information"},
    {"help", 'h', "extended usage information"},
    {"disassemble", 'd',
     "readable bytecode representation with labels, globals and comments"},
    {"block-allocator", 'b',
     "use block allocator instead of garbage collection"},
    {"aot-functions", 'a', "compile all functions to machine code"},
    {"memory-usage", 'm',
     "display the memory usage of parsing, compilation and the virtual "
     "machine"},
};

void usage() {
  Str prefix = STRING("usage: purple_garden");
  printf("%.*s ", (int)prefix.len, prefix.p);
  size_t len = sizeof(options) / sizeof(cli_option);
  for (size_t i = 0; i < len; i++) {
    printf("[-%c | --%s] ", options[i].name_short, options[i].name_long);
    if ((i + 1) % 3 == 0 && i + 1 < len) {
      printf("\n%*.s ", (int)prefix.len, "");
    }
  }
  printf("<file.garden>\n");
}

Args Args_parse(int argc, char **argv) {
  Args a = (Args){0};
  // MUST be in sync with options, otherwise this will not work as intended
  struct option long_options[] = {
      {options[0].name_long, no_argument, &a.version, 1},
      {options[1].name_long, no_argument, &a.help, 1},
      {options[2].name_long, no_argument, &a.disassemble, 1},
      {options[3].name_long, no_argument, &a.block_allocator, 1},
      {options[4].name_long, no_argument, &a.aot_functions, 1},
      {options[5].name_long, no_argument, &a.memory_usage, 1},
      {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "vhdbam", long_options, NULL)) != -1) {
    switch (opt) {
    case 'v':
      a.version = 1;
      break;
    case 'h':
      a.help = 1;
      break;
    case 'd':
      a.disassemble = 1;
      break;
    case 'b':
      a.block_allocator = 1;
      break;
    case 'a':
      a.aot_functions = 1;
      break;
    case 'm':
      a.memory_usage = 1;
      break;
    case 0:
      break;
    default:
      usage();
      exit(EXIT_FAILURE);
    }
  }

  if (optind < argc) {
    a.filename = argv[optind];
  }

  // command handling
  if (a.version) {
    printf("purple_garden: %s-%s-%s\n", CTX, VERSION, COMMIT);
    exit(EXIT_SUCCESS);
  } else if (a.help) {
    usage();
    size_t len = sizeof(options) / sizeof(cli_option);
    printf("\noptions:\n");
    for (size_t i = 0; i < len; i++) {
      printf("\t-%c, --%-15s %s\n", options[i].name_short, options[i].name_long,
             options[i].description);
    }
    exit(EXIT_SUCCESS);
  }

  if (a.filename == NULL) {
    usage();
    fprintf(stderr, "error: Missing a file? try `-h/--help`\n");
    exit(EXIT_FAILURE);
  };

  return a;
}

#if BENCH
#define BENCH_PUTS(msg)                                                        \
  {                                                                            \
    gettimeofday(&end_time, NULL);                                             \
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +              \
                          (end_time.tv_usec - start_time.tv_usec) / 1000000.0; \
    printf("[%10.4fms] %s\n", elapsed_time * 1000, msg);                       \
    gettimeofday(&start_time, NULL);                                           \
  }
#else
#define BENCH_PUTS(msg)
#endif

int main(int argc, char **argv) {
#if BENCH
  struct timeval start_time, end_time;
  gettimeofday(&start_time, NULL);
#endif
  Args a = Args_parse(argc, argv);

  BENCH_PUTS("main::Args_parse: Parsed arguments");

  Str input = IO_read_file_to_string(a.filename);
#if DEBUG
  puts("================== INPUTS ==================");
  Str_debug(&input);
#endif
  BENCH_PUTS("io::IO_read_file_to_string: mmaped input");

  Lexer l = Lexer_new(input);

  Allocator parser_alloc = {
      .init = bump_init,
      .request = bump_request,
      .destroy = bump_destroy,
      .reset = bump_reset,
      .stats = bump_stats,
  };
  parser_alloc.ctx = parser_alloc.init(sizeof(Node) * input.len / 2);
  Parser p = Parser_new(&l, &parser_alloc);

  Vm vm = cc(&p);
#if DEBUG
  puts("================= DISASM =================");
  a.disassemble = 1;
#endif
  if (a.disassemble) {
    disassemble(&vm);
    puts("");
  }
  BENCH_PUTS("cc::cc: Flattened AST to byte code");

  if (a.memory_usage) {
    Stats s = parser_alloc.stats(parser_alloc.ctx);
    double percent = (s.current * 100) / (double)s.allocated;
    printf("parsing: %.3f KB of %.3f KB used (%f%%)\n", s.current / 1024.0,
           s.allocated / 1024.0, percent);
    // TODO: add compilation usage here
    // TODO: add virtual usage here
  }

#if DEBUG
  puts("================= MEMORY =================");
  Stats s = parser_alloc.stats(parser_alloc.ctx);
  printf("%.3f KB of %.3f KB used\n", s.current / 1024.0, s.allocated / 1024.0);
#endif

  parser_alloc.destroy(parser_alloc.ctx);
  BENCH_PUTS("mem::Allocator::destroy: Deallocated AST memory space");

  int runtime_code = Vm_run(&vm);
  BENCH_PUTS("vm::Vm_run: Walked and executed byte code");

  Vm_destroy(vm);
  BENCH_PUTS("vm::Vm_destroy: Deallocated global pool and bytecode list");
  munmap(input.p, input.len);

  return runtime_code == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
