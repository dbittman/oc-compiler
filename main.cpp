/* main program file for oc */
#include <string>
#include <vector>
using namespace std;

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "stringset.h"
#include "oc.h"

char *progname = NULL;

vector<string> defines;

int debug_yylex = 0, debug_yyparse = 0;

void usage()
{
    fprintf(stderr, "usage: %s [-D <define>] [-yl] <source file>\n", progname);
    exit(0);
}

int main (int argc, char** argv) {
    FILE *outfile = NULL;
    /* basic init stuff for auxlib */
    progname = argv[0];

    int c;
    while((c = getopt(argc, argv, "D:h@ly")) != -1) {
        switch(c) {
            case 'D':
                defines.push_back(string(optarg));
                break;
            case 'h':
                usage();
                break;
            case '@':break;
            case 'l':
                debug_yylex = 1;
                break;
            case 'y':
                debug_yyparse = 1;
                break;
        }
    }

    if(optind == argc) {
        errprintf("no program file specified\n");
        return 1;
    }
    if(optind + 1 < argc) {
        errprintf("multiple program files is not supported\n");
        return 1;
    }
    
    char *infilename  = argv[optind];
    string outfilename = string(infilename);
    size_t found = outfilename.find_last_of(".");
    if(found != string::npos) {
        outfilename = outfilename.substr(0, found);
    }
    outfilename += ".str";
    outfile = fopen(outfilename.c_str(), "w");
    if(!outfile) {
        perror("failed to open output file");
        return 1;
    }
    oc_cpp_parse(outfile, &defines, infilename);
    dump_stringset(outfile);
    return 0;
}

