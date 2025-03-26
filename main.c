#include <getopt.h>
#include <stdlib.h>

#include "cc.h"
#include "common.h"
#include "io.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

#define VERSION "alpha"
#define TAG "first"

typedef struct {
  // use block allocator before starting a garden, instead of gc; int because
  // getopt has no bool support
  int alloc_block;
  // enable debug logs
  int debug;
  int version;
  // entry garden
  char *filename;
} Args;

static const char *options[] = {
    "alloc-block",
    "debug",
    "version",
};

void usage() {
  fprintf(stderr, "usage: purple_garden ");
  size_t len = sizeof(options) / sizeof(char *);
  for (size_t i = 0; i < len; i++) {
    fprintf(stderr, "[--%s] ", options[i]);
  }
  fprintf(stderr, "<file.garden>\n");
}

Args Args_parse(int argc, char **argv) {
  Args a = (Args){0};
  struct option long_options[] = {{options[0], no_argument, &a.alloc_block, 1},
                                  {options[1], no_argument, &a.debug, 1},
                                  {options[2], no_argument, &a.version, 1},
                                  {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
    switch (opt) {
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

  return a;
}

int main(int argc, char **argv) {
  Args a = Args_parse(argc, argv);
  if (a.version) {
    fprintf(stderr, "purple_garden: %s-%s\n", VERSION, TAG);
    return EXIT_SUCCESS;
  }
  if (a.filename == NULL) {
    usage();
    ASSERT(a.filename != NULL,
           "Wanted a filename as an argument, not enough arguments")
  }
  String input = IO_read_file_to_string(a.filename);

  Lexer l = Lexer_new(input);
  Parser p = Parser_new(&l);
  Node ast = Parser_run(&p);
  Vm vm = cc(&ast);
  Vm_run(&vm);
  Node_destroy(&ast);
  Vm_destroy(vm);
  free(input.p);

  return EXIT_SUCCESS;
}
