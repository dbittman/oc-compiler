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

