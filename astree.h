#ifndef __ASTREE_H__
#define __ASTREE_H__

#include <string>
#include <vector>
using namespace std;

#include "type.h"
#include "auxlib.h"

struct symbol;

struct astree {
    int symbol;               // token code
    size_t filenr;            // index into filename stack
    size_t linenr;            // line number from source code
    size_t offset;            // offset of token with current line
    const string* lexinfo;    // pointer to lexical information
    vector<astree*> children; // children of this n-way node
    struct astree *parent;
    struct symbol *symentry;
    attr_bitset attributes;
    const string *type_name;
    int blocknr;
};


astree* new_astree (int symbol, int filenr, int linenr,
        int offset, const char* lexinfo);
astree* adopt1 (astree* root, astree* child);
astree* adopt2 (astree* root, astree* left, astree* right);
astree* adopt1sym (astree* root, astree* child, int symbol);
void dump_astree (FILE* outfile, astree* root);
void yyprint (FILE* outfile, unsigned short toknum,
        astree* yyvaluep);
void free_ast (astree* tree);
void free_ast2 (astree* tree1, astree* tree2);

astree* tree_function(astree* ident, astree *arglist, astree* block);
extern const char *attr_names[ATTR_bitset_size];
#endif
