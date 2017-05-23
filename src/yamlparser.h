/*
 *  VMOD YAML Config Parser
 */

// system includes
#include <stdlib.h>
#ifdef DEBUG_PARSER
  #include <stdio.h>
#endif
#include <yaml.h>

// global parsed options
typedef struct __global_options {
  unsigned int gc_interval;
  unsigned int partitions;
} goptions;

enum parse_expect_type {
  PARSE_EXPECT_ID = 0,
  PARSE_EXPECT_VALUE
};

// main global options
goptions global_opts;

// global parser objects
static yaml_parser_t main_parser;

// open a file from the filesystem
FILE *load_yaml_file(const char *filename);

// parse yaml file..
int parse_yaml_file(FILE *handle);

// close file handle
int close_yaml_file(FILE *handle);
