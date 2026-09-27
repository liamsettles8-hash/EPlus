#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
int run_eplus(const char *s);
int main(int argc,char **argv){char *s;if(argc<2){fprintf(stderr,"E#+ engine 0.1\nUsage: eplus-engine file.eplus\n");return 1;}s=read_entire_file(argv[1]);if(!s){fprintf(stderr,"E#+ error: could not read file\n");return 1;}int r=run_eplus(s);free(s);return r;}
