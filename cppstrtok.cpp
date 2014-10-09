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


// Run cpp against the lines of the file.
static void cpplines (FILE* pipe, char* filename)
{
    int linenr = 1;
    char inputname[LINESIZE];
    strcpy (inputname, filename);
    for (;;) {
        char buffer[LINESIZE];
        char* fgets_rc = fgets (buffer, LINESIZE, pipe);
        if (fgets_rc == NULL) break;
        chomp (buffer, '\n');
       // printf ("%s:line %d: [%s]\n", filename, linenr, buffer);
        // http://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html
        int sscanf_rc = sscanf (buffer, "# %d \"%[^\"]\"",
                &linenr, filename);
        if (sscanf_rc == 2) {
            //printf ("DIRECTIVE: line %d file \"%s\"\n", linenr, filename);
            continue;
        }
        char* savepos = NULL;
        char* bufptr = buffer;
        for (int tokenct = 1;; ++tokenct) {
            char* token = strtok_r (bufptr, " \t\n", &savepos);
            bufptr = NULL;
            if (token == NULL) break;
        //    printf ("token %d.%d: [%s]\n",
          //          linenr, tokenct, token);
            intern_stringset(token);
            //DEBUG("intern (\"%s\") returned %p->\"%s\"\n",
              //                          token, str->c_str(), str->c_str());
        }
        ++linenr;
    }
}

int oc_cpp_parse(vector<string> *defines, char *filename)
{
    string arguments = "";
    for(auto it = defines->begin(); it != defines->end(); ++it) {
        arguments += "-D" + *it + " ";
    }
    string command = CPP + " " + arguments + " " + filename;
    //printf ("command=\"%s\"\n", command.c_str());
    FILE* pipe = popen (command.c_str(), "r");
    if (pipe == NULL) {
        errprintf ("command %s failed!\n", command.c_str());
    } else {
        cpplines (pipe, filename);
        return pclose (pipe);
    }
    return -1;
}

