#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
char *read_entire_file(const char *path){FILE *f=fopen(path,"rb");long n;char *p;if(!f)return NULL;fseek(f,0,SEEK_END);n=ftell(f);fseek(f,0,SEEK_SET);p=(char*)malloc((size_t)n+1);if(!p){fclose(f);return NULL;}fread(p,1,(size_t)n,f);p[n]=0;fclose(f);return p;}
