/*
 *  VMOD YAML Config Parser
 */

#include "yamlparser.h"

// open a file from the filesystem
FILE *load_yaml_file(const char *filename) {
  // open file for reading...
  FILE *yaml_file;
  yaml_file = fopen(filename, "r");

  #ifdef DEBUG_PARSER
    printf("yamlparser.c :: load_yaml_file(): Open handle is at 0x%X\n", yaml_file);
  #endif

  // return handler
  return yaml_file;
}

// parse yaml file..
int parse_yaml_file(FILE *handle) {
  // parser state
  unsigned int state = PARSE_EXPECT_ID;
  unsigned int *data_pointer = NULL;

  // initialize yaml parser
  if (!yaml_parser_initialize(&main_parser)) {
    return -1;
  }

  // yaml token unit
  yaml_event_t pevent;

  // associate parser with file handler
  yaml_parser_set_input_file(&main_parser, handle);
  #ifdef DEBUG_PARSER
    printf("yamlparser.c :: parse_yaml_file(): Linking open handle 0x%X with YAML parser 0x%X\n", handle, &main_parser);
  #endif

  // parse yaml content
  do {
    // parse yaml file
    if (!yaml_parser_parse(&main_parser, &pevent)) {
      //parser error
      return -1;
    }

    // analyze oken type
    switch (pevent.type) {
      case YAML_STREAM_START_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): START OF YAML STREAM\n");
        #endif
        break;
      case YAML_STREAM_END_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): END OF YAML STREAM\n");
        #endif
        break;
      case YAML_DOCUMENT_START_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): -> START OF YAML DOCUMENT\n");
        #endif
        break;
      case YAML_DOCUMENT_END_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): -> END OF YAML DOCUMENT\n");
        #endif
        break;
      case YAML_SEQUENCE_START_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): --> Start of a Sequence\n");
        #endif
        break;
      case YAML_SEQUENCE_END_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): --> End of a Sequence\n");
        #endif
        break;
      case YAML_MAPPING_START_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): --> Start of a Mapping\n");
        #endif
        break;
      case YAML_MAPPING_END_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): --> End of a Mapping\n");
        #endif
        break;
      case YAML_ALIAS_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): ---> Alias Event (anchor %s)\n", pevent.data.alias.anchor);
        #endif
        break;
      case YAML_SCALAR_EVENT:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): ---> Scalar Event (value %s)\n", pevent.data.scalar.value);
        #endif
        if (state == PARSE_EXPECT_ID) {
          if (strncmp(pevent.data.scalar.value, "gc_interval", strlen("gc_interval")) == 0) {
            #ifdef DEBUG_PARSER
              printf("yamlparser.c :: parse_yaml_file(): ----> Selecting structure member at address 0x%X\n", &(global_opts.gc_interval));
            #endif
            data_pointer = &(global_opts.gc_interval);
          } else if (strncmp(pevent.data.scalar.value, "partitions", strlen("partitions")) == 0) {
            #ifdef DEBUG_PARSER
              printf("yamlparser.c :: parse_yaml_file(): ----> Selecting structure member at address 0x%X\n", &(global_opts.partitions));
            #endif
            data_pointer = &(global_opts.partitions);
          } else data_pointer = NULL;
          #ifdef DEBUG_PARSER
            printf("yamlparser.c :: parse_yaml_file(): ----> Switching state to PARSE_EXPECT_VALUE\n");
          #endif
          state = PARSE_EXPECT_VALUE;
        } else {
          if (data_pointer != NULL) {
            #ifdef DEBUG_PARSER
              printf("yamlparser.c :: parse_yaml_file(): Copying value %d to address 0x%X...\n", atoi(pevent.data.scalar.value), data_pointer);
            #endif
            *data_pointer = atoi(pevent.data.scalar.value);
          }
          #ifdef DEBUG_PARSER
            printf("yamlparser.c :: parse_yaml_file(): ----> Switching state to PARSE_EXPECT_ID\n");
          #endif
          state = PARSE_EXPECT_ID;
        }
        break;
      default:
        #ifdef DEBUG_PARSER
          printf("yamlparser.c :: parse_yaml_file(): Parsed YAML event of type %d\n", pevent.type);
        #endif
        break;
    }

    if (pevent.type != YAML_STREAM_END_EVENT)
      yaml_event_delete(&pevent);

  } while (pevent.type != YAML_STREAM_END_EVENT);
  //remove parsed token
  yaml_event_delete(&pevent);

  // destroy parser
  yaml_parser_delete(&main_parser);

  return 0;
}

// close file handle
int close_yaml_file(FILE *handle) {
  // close file handler
  if (handle != NULL) {
    #ifdef DEBUG_PARSER
      printf("yamlparser.c :: close_yaml_file(): Closing handle at 0x%X\n", handle);
    #endif

    fclose(handle);
    return 0;
  } else return 1;
}

