#ifndef __OC_H
#define __OC_H

#include <vector>

#include <stdio.h>


extern char *progname;

#define errprintf(str...) \
    do { \
        fprintf(stderr, "%s: ", progname); \
        fprintf(stderr, str); \
    } while(0);

int oc_cpp_parse(std::vector<string> *, char*);

#endif

