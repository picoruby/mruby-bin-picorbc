#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#ifndef MRBC_ALLOC_LIBC
#define MRBC_ALLOC_LIBC
#endif

#include <picorbc.h>
#include <common.h>

#define MRB_DUMP_OK                     0
#define MRB_DUMP_GENERAL_FAILURE      (-1)
#define MRB_DUMP_WRITE_FAULT          (-2)
#define MRB_DUMP_READ_FAULT           (-3)
#define MRB_DUMP_INVALID_FILE_HEADER  (-4)
#define MRB_DUMP_INVALID_IREP         (-5)
#define MRB_DUMP_INVALID_ARGUMENT     (-6)
int
mrb_dump_irep_cstruct(uint8_t flags, FILE *fp, const char *initname)
//mrb_dump_irep_cstruct(mrb_state *mrb, const mrb_irep *irep, uint8_t flags, FILE *fp, const char *initname)
{
  if (fp == NULL || initname == NULL || initname[0] == '\0') {
    return MRB_DUMP_INVALID_ARGUMENT;
  }
  if (fprintf(fp, "#include <mruby.h>\n"
                  "#include <mruby/proc.h>\n"
                  "#include <mruby/presym.h>\n"
                  "\n") < 0) {
    return MRB_DUMP_WRITE_FAULT;
  }
  fputs("#define mrb_BRACED(...) {__VA_ARGS__}\n", fp);
  fputs("#define mrb_DEFINE_SYMS_VAR(name, len, syms, qualifier) \\\n", fp);
  fputs("  static qualifier mrb_sym name[len] = mrb_BRACED syms\n", fp);
  fputs("\n", fp);
//  mrb_value init_syms_code = mrb_str_new_capa(mrb, 0);
//  int max = 1;
//  int n = dump_irep_struct(mrb, irep, flags, fp, initname, 0, init_syms_code, &max);
//  if (n != MRB_DUMP_OK) return n;
  fprintf(fp, "#ifdef __cplusplus\nextern const struct RProc %s[];\n#endif\n", initname);
  fprintf(fp, "const struct RProc %s[] = {{\n", initname);
  fprintf(fp, "NULL,NULL,MRB_TT_PROC,7,0,{&%s_irep_0},NULL,{NULL},\n}};\n", initname);
  fputs("static void\n", fp);
  fprintf(fp, "%s_init_syms(mrb_state *mrb)\n", initname);
  fputs("{\n", fp);
//  fputs(RSTRING_PTR(init_syms_code), fp);
  fputs("}\n", fp);
  return MRB_DUMP_OK;
}


int loglevel;

int handle_opt(int argc, char * const *argv, char *out, char *b_symbol)
{
  struct option longopts[] = {
    { "version",  no_argument,       NULL, 'v' },
    { "debug",    no_argument,       NULL, 'd' },
    { "verbose",  no_argument,       NULL, 'b' },
    { "loglevel", required_argument, NULL, 'l' },
    { "",         required_argument, NULL, 'B' },
    { "",         required_argument, NULL, 'o' },
    { "",         no_argument,       NULL, 'E' },
    { "remove-lv",no_argument,       NULL, 'R' },
    { "",         no_argument,       NULL, 'S' },
    { 0,          0,                 0,     0  }
  };
  int opt;
  int longindex;
  loglevel = LOGLEVEL_INFO;
  while ((opt = getopt_long(argc, argv, "vdblERS:B:o:", longopts, &longindex)) != -1) {
    switch (opt) {
      case 'v':
        fprintf(stdout, "PicoRuby compiler %s\n", PICORBC_VERSION);
        return -1;
      case 'b': /* verbose */
        /* TODO */
        break;
      case 'd': /* debug */
        #ifndef PICORBC_DEBUG
          fprintf(stderr, "[ERROR] `--debug` option is only valid if you did `make` without CFLAGS=-DNDEBUG\n");
          return 1;
        #endif
        loglevel = LOGLEVEL_DEBUG;
        break;
      case 'l':
        #ifndef PICORBC_DEBUG
          fprintf(stderr, "[ERROR] `--loglevel=[level]` option is only valid if you made executable without -DNDEBUG\n");
          return 1;
        #endif
        if ( !strcmp(optarg, "debug") ) { loglevel = LOGLEVEL_DEBUG; } else
        if ( !strcmp(optarg, "info") )  { loglevel = LOGLEVEL_INFO; } else
        if ( !strcmp(optarg, "warn") )  { loglevel = LOGLEVEL_WARN; } else
        if ( !strcmp(optarg, "error") ) { loglevel = LOGLEVEL_ERROR; } else
        if ( !strcmp(optarg, "fatal") ) { loglevel = LOGLEVEL_FATAL; } else
        {
          fprintf(stderr, "Invalid loglevel option: %s\n", optarg);
          return 1;
        }
        break;
      case 'B':
        strsafecpy(b_symbol, optarg, 254);
        break;
      case 'o':
        strsafecpy(out, optarg, 254);
        break;
      case 'E':
      case 'R':
      case 'S':
        break;
      default:
        fprintf(stderr, "error! \'%c\' \'%c\'\n", opt, optopt);
        return 1;
    }
  }
  return 0;
}

const char C_FORMAT_LINES[5][22] = {
  "#include <stdint.h>",
  "#ifdef __cplusplus",
  "extern const uint8_t ",
  "#endif",
  "const uint8_t ",
};

int output(Scope *scope, char *in, char *out, char *b_symbol)
{
  FILE *fp;
  if (out[0] == '\0') {
    if (strcmp(&in[strlen(in) - 3], ".rb") == 0) {
      memcpy(out, in, strlen(in));
      (b_symbol[0] == '\0') ?
        memcpy(&out[strlen(in) - 3], ".mrb\0", 5) :
        memcpy(&out[strlen(in) - 3], ".c\0", 3);
    } else {
      memcpy(out, in, strlen(in));
      (b_symbol[0] == '\0') ?
        memcpy(&out[strlen(in)], ".mrb\0", 5) :
        memcpy(&out[strlen(in)], ".c\0", 3);
    }
  }
  if( (fp = fopen( out, "wb" ) ) == NULL ) {
    FATALP("picorbc: cannot write a file. (%s)", out);
    return 1;
  } else {
    if (b_symbol[0] == '\0') {
      fwrite(scope->vm_code, scope->vm_code_size, 1, fp);
    } else {
      int i;
      for (i=0; i < 5; i++) {
        fwrite(C_FORMAT_LINES[i], strlen(C_FORMAT_LINES[i]), 1, fp);
        if (i == 2) {
          fwrite(b_symbol, strlen(b_symbol), 1, fp);
          fwrite("[];", 3, 1, fp);
        }
        if (i < 4) fwrite("\n", 1, 1, fp);
      }
      fwrite(b_symbol, strlen(b_symbol), 1, fp);
      fwrite("[] = {", 6, 1, fp);
      char buf[6];
      for (i = 0; i < scope->vm_code_size; i++) {
        if (i % 16 == 0) fwrite("\n", 1, 1, fp);
        snprintf(buf, 6, "0x%02x,", scope->vm_code[i]);
        fwrite(buf, 5, 1, fp);
      }
      fwrite("\n};", 3, 1, fp);
    }
    fclose(fp);
  }
  return 0;
}

#define BUFSIZE 1024

int main(int argc, char * const *argv)
{
  char out[255];
  out[0] = '\0';
  char b_symbol[255];
  b_symbol[0] = '\0';
  int ret = handle_opt(argc, argv, out, b_symbol);
  if (ret != 0) return ret;

  if ( !argv[optind] ) {
    ERRORP("picorbc: no program file given");
    return 1;
  }

  char *in;
  { /* Concatenate files or single file */
    FILE *infp, *outfp;
    int i;
    char buf[BUFSIZE];
    int readsize;
    if (!argv[optind + 1]) { /* sigle input file */
      in = argv[optind];
    } else {                 /* multiple input files */
      in = tmpnam(NULL);
      if (!in) {
        FATALP("Could't get tempfile name");
        return -1;
      }
      outfp = fopen(in, "wb");
      if (!outfp) {
        FATALP("Could't open tempfile");
        return -1;
      }
      i = optind;
      while (argv[i]) {
        infp = fopen(argv[i], "rb");
        if (!infp) {
          FATALP("Could't open file: %s", argv[i]);
          return -1;
        }
        while ((readsize = fread(buf, 1, BUFSIZE, infp)) != 0)fwrite(buf, 1, readsize, outfp);
        fclose(infp);
        i++;
      }
      fclose(outfp);
    }
  }

  StreamInterface *si = StreamInterface_new(in, STREAM_TYPE_FILE);
  if (si == NULL) return 1;
  ParserState *p = Compiler_parseInitState(si->node_box_size);
  if (Compiler_compile(p, si)) {
    ret = output(p->scope, in, out, b_symbol);
  } else {
    ret = 1;
  }
  StreamInterface_free(si);
  Compiler_parserStateFree(p);
  if (ret != 0) return ret;
  return ret;
}
