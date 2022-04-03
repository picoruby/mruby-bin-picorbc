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
#include <dump.h>
#include <context.h>

int loglevel;

struct picorbc_args {
  const char *prog;
  char *outfile;
  char *initname;
  char **argv;
  int argc;
  bool verbose;
  bool dump_struct;
  uint8_t flags;
};

int handle_opt(struct picorbc_args *args)
{
  struct option longopts[] = {
    { "version",  no_argument,       NULL, 'v' },
    { "verbose",  no_argument,       NULL, 'V' },
    { "loglevel", required_argument, NULL, 'l' },
    { "",         required_argument, NULL, 'B' },
    { "",         required_argument, NULL, 'o' },
    { "",         no_argument,       NULL, 'S' },
    { 0,          0,                 0,     0  }
  };
  int opt;
  int longindex;
  loglevel = LOGLEVEL_INFO;
  while ((opt = getopt_long(args->argc, args->argv, "vVlSB:o:", longopts, &longindex)) != -1) {
    switch (opt) {
      case 'v':
        fprintf(stdout, "PicoRuby compiler %s", PICORBC_VERSION);
        #ifdef PICORUBY_DEBUG
          fprintf(stdout, " (debug build)");
        #endif
        fprintf(stdout, "\n");
        return -1;
      case 'V': /* verbose */
        args->verbose = true;
        break;
      case 'l':
        if ( !strcmp(optarg, "debug") ) { loglevel = LOGLEVEL_DEBUG; } else
        if ( !strcmp(optarg, "info") )  { loglevel = LOGLEVEL_INFO; } else
        if ( !strcmp(optarg, "warn") )  { loglevel = LOGLEVEL_WARN; } else
        if ( !strcmp(optarg, "error") ) { loglevel = LOGLEVEL_ERROR; } else
        if ( !strcmp(optarg, "fatal") ) { loglevel = LOGLEVEL_FATAL; } else
        {
          ERRORP("picorbc: invalid loglevel option `%s`", optarg);
          return 1;
        }
        break;
      case 'B':
        strsafecpy(args->initname, optarg, 254);
        break;
      case 'o':
        strsafecpy(args->outfile, optarg, 254);
        break;
      case 'S':
        args->dump_struct = true;
        break;
      default:
        ERRORP("error! \'%c\' \'%c\'", opt, optopt);
        return 1;
    }
  }
  args->prog = args->argv[optind];
  if (!args->prog) {
    ERRORP("picorbc: no program file given");
    return 1;
  }
  if (args->argv[optind + 1] && args->outfile[0] == '\0') {
    ERRORP("picorbc: output file should be specified to compile multiple files");
    return 1;
  }
  if (args->outfile[0] != '\0') return 0;
  if (strcmp(&args->prog[strlen(args->prog) - 3], ".rb") == 0) {
    memcpy(args->outfile, args->prog, strlen(args->prog));
    (args->initname[0] == '\0') ?
      memcpy(&args->outfile[strlen(args->prog) - 3], ".mrb\0", 5) :
      memcpy(&args->outfile[strlen(args->prog) - 3], ".c\0", 3);
  } else {
    memcpy(args->outfile, args->prog, strlen(args->prog));
    (args->initname[0] == '\0') ?
      memcpy(&args->outfile[strlen(args->prog)], ".mrb\0", 5) :
      memcpy(&args->outfile[strlen(args->prog)], ".c\0", 3);
  }
  return 0;
}

#define BUFSIZE 1024

int main(int argc, char * const *argv)
{
  struct picorbc_args args = {0};
  args.argv = (char **)argv;
  args.argc = argc;
  char outfile[255];
  char initname[255] = {0};
  args.outfile = outfile;
  args.initname = initname;
  int ret = handle_opt(&args);
  if (ret != 0) return ret;

  char *in = NULL;
  FILE *concatfile = NULL;
  { /* Concatenate files or single file */
    FILE *infp;
    char buf[BUFSIZE];
    int readsize;
    if (!argv[optind + 1]) { /* sigle input file */
      in = argv[optind];
    } else {                 /* multiple input files */
      concatfile = tmpfile();
      if (!concatfile) {
        FATALP("Could't open tempfile");
        return -1;
      }
      int i = optind;
      while (argv[i]) {
        infp = fopen(argv[i], "rb");
        if (!infp) {
          FATALP("Could't open file: %s", argv[i]);
          return -1;
        }
        while ((readsize = fread(buf, 1, BUFSIZE, infp)) != 0)
          fwrite(buf, 1, readsize, concatfile);
        fclose(infp);
        i++;
      }
      if (fseek(concatfile, 0L, SEEK_SET) != 0) {
        FATALP("Could't rewind file");
        return -1;
      }
    }
  }

  StreamInterface *si = StreamInterface_new(concatfile, in, STREAM_TYPE_FILE);
  if (si == NULL) return 1;
  ParserState *p = Compiler_parseInitState(si->node_box_size);
  p->verbose = args.verbose;
  picorbc_context *c = picorbc_context_new();
  if (Compiler_compile(p, si, c)) { /* TODO picorbc_context */
    FILE *fp;
    if (strcmp("-", args.outfile) == 0) {
      fp = stdout;
    } else if ( (fp = fopen(args.outfile, "wb") ) == NULL ) {
      FATALP("picorbc: cannot write a file. (%s)", args.outfile);
      return 1;
    }
    if (args.dump_struct && args.initname[0] != '\0') {
      ret = Dump_cstructDump(fp, p->scope, args.flags, args.initname);
    } else {
      ret = Dump_mrbDump(fp, p->scope, args.initname);
    }
    fclose(fp);
  } else {
    ret = 1;
  }
  fclose(concatfile);
  StreamInterface_free(si);
  Compiler_parserStateFree(p);
  return ret;
}
