/* main program file for oc.
 * Daniel Bittman (dbittman)
 */
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
#include "lyutils.h"
#include "auxlib.h"

char *progname = NULL;

/* this contains a list of all flags supplied by -D. */
vector<string> defines;
/* other debugging flags */
int debug_yyparse = 0;

extern char *yytext;
extern FILE *yyin;
extern int yylex();
extern int yy_flex_debug;

void usage()
{
    fprintf(stderr, "usage: %s [-D <define>] [-yl] <source file>\n",
            progname);
    exit(0);
}

int main (int argc, char** argv) {
    FILE *outfile = NULL;
    /* basic init stuff for auxlib */
    progname = argv[0];
    set_execname(progname);
    yy_flex_debug = 0;

    int c;
    /* holy... */
    while((c = getopt(argc, argv, "D:h@ly")) != -1) {
        switch(c) {
            case 'D':
                defines.push_back(string(optarg));
                break;
            case 'h':
                usage();
                break;
            /* '@' is implementation specific, so this is valid */
            case '@':break;
            case 'l':
                yy_flex_debug = 1;
                break;
            case 'y':
                debug_yyparse = 1;
                break;
        }
    }

    /* check for the right number of remaining options */
    if(optind == argc) {
        oc_errprintf("no program file specified\n");
        return 1;
    }
    if(optind + 1 < argc) {
        oc_errprintf("multiple program files is not supported\n");
        return 1;
    }
    
    /* generate output file name */
    char *infilename  = argv[optind];
    string filename = string(infilename);
    /* check if we even have an extension, and if so, if it's correct */
    size_t found = filename.find_last_of(".");
    if(found == string::npos || filename.substr(found) != ".oc") {
        oc_errprintf("file '%s' has a non-allowed file extension!\n",
                infilename);
        return 1;
    }
    /* append the new extension */
    filename = filename.substr(0, found);
    filename = string(basename(filename.c_str()));
    string stroutfile = filename + ".str";
    string tokoutfile = filename + ".tok";

    /* test for access to input file.
     * Yeah, we could call access(), but I'm lazy. */
    FILE *infile = fopen(infilename, "r");
    if(!infile) {
        perror("could not open input file");
        return 1;
    }
    /* we don't directly read from infile, so close the handle */
    fclose(infile);

    /* call the "scanner" */
    FILE *cpp_pipe = oc_cpp_getfile(&defines, infilename);
    if(!cpp_pipe)
        return 1;
    
    yyin = cpp_pipe;
    outfile = fopen(tokoutfile.c_str(), "w");
    if(!outfile) {
        perror("failed to open output .tok file");
        return 1;
    }
    scanner_scan(outfile);
    fclose(outfile);

    /* and write out the stringset to the output file */
    outfile = fopen(stroutfile.c_str(), "w");
    if(!outfile) {
        perror("failed to open output file");
        return 1;
    }
    dump_stringset(outfile);
    fclose(outfile);
    return 0;
}

