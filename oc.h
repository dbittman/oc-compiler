#ifndef __OC_H
#define __OC_H

#include <vector>

#include <stdio.h>

extern char *progname;

#define oc_errprintf(str...) \
    do { \
        fprintf(stderr, "%s: ", progname); \
        fprintf(stderr, str); \
    } while(0);

FILE *oc_cpp_getfile(std::vector<std::string> *defines, char *filename);
int scanner_scan(FILE *outf);

#endif

