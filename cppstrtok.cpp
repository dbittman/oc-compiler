// Use cpp to scan a file and print line numbers.
// Print out each input line read in, then strtok it for
// tokens.

#include <string>
#include <vector>
using namespace std;

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>

#include "oc.h"
#include "stringset.h"

const string CPP = "/usr/bin/cpp";
const size_t LINESIZE = 1024;

// Chomp the last character from a buffer if it is delim.
static void chomp (char* string, char delim)
{
    size_t len = strlen (string);
    if (len == 0) return;
    char* nlpos = string + len - 1;
    if (*nlpos == delim) *nlpos = '\0';
}

/* calls the C pre-processor, tokenizes the output,
 * and adds it to the stringset. */
static void cpplines (FILE* pipe, char* filename)
{
    int linenr = 1;
    char inputname[LINESIZE];
    strcpy (inputname, filename);
    for (;;) {
        /* get the line */
        char buffer[LINESIZE];
        char* fgets_rc = fgets (buffer, LINESIZE, pipe);
        if (fgets_rc == NULL) break;
        /* remove the end whitespace */
        chomp (buffer, '\n');
        /* check for pre-processor directives */
        int sscanf_rc = sscanf (buffer, "# %d \"%[^\"]\"",
                &linenr, filename);
        if (sscanf_rc == 2) {
            continue;
        }
        /* tokenize the line */
        char* savepos = NULL;
        char* bufptr = buffer;
        for (int tokenct = 1;; ++tokenct) {
            char* token = strtok_r (bufptr, " \t\n", &savepos);
            bufptr = NULL;
            if (token == NULL) break;
            /* add to stringset */
            intern_stringset(token);
        }
        ++linenr;
    }
}

FILE *oc_cpp_getfile(vector<string> *defines, char *filename)
{
    string arguments = "";
    /* create the argument list from -D options */
    for(auto it = defines->begin(); it != defines->end(); ++it) {
        arguments += "-D" + *it + " ";
    }
    /* create the command */
    string command = CPP + " " + arguments + " " + filename;
    /* use popen to pipe/fork/exec */
    FILE* pipe = popen (command.c_str(), "r");
    if (pipe == NULL) {
        oc_errprintf ("command %s failed!\n", command.c_str());
    } else {
        /* the command succeeded, so read the CPP output
         * and close the pipe */
        return pipe;
    }
    return 0;
}

