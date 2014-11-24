#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astree.h"
#include "stringset.h"
#include "lyutils.h"
#include "type.h"
#include "semantics.h"


astree* new_astree (int symbol, int filenr, int linenr,
                    int offset, const char* lexinfo) {
   astree* tree = new astree();
   tree->symbol = symbol;
   tree->filenr = filenr;
   tree->linenr = linenr;
   tree->offset = offset;
   tree->lexinfo = intern_stringset (lexinfo);
   tree->blocknr = 0;
   DEBUGF ('f', "astree %p->{%d:%d.%d: %s: \"%s\"}\n",
           tree, tree->filenr, tree->linenr, tree->offset,
           get_yytname (tree->symbol), tree->lexinfo->c_str());
   return tree;
}

astree* adopt1 (astree* root, astree* child) {
   root->children.push_back (child);
   child->parent = root;
   DEBUGF ('a', "%p (%s) adopting %p (%s)\n",
           root, root->lexinfo->c_str(),
           child, child->lexinfo->c_str());
   return root;
}

astree* adopt2 (astree* root, astree* left, astree* right) {
   adopt1 (root, left);
   adopt1 (root, right);
   return root;
}

astree* adopt1sym (astree* root, astree* child, int symbol) {
   root = adopt1 (root, child);
   root->symbol = symbol;
   return root;
}

astree* tree_function(astree* ident, astree *arglist, astree* block) {
    int prototype = block->symbol == ';';
    astree* function = new_astree(prototype ?
                TOK_PROTOTYPE : 
                TOK_FUNCTION,
            ident->filenr, ident->linenr,
            ident->offset, prototype ? 
                "<<PROTOTYPE>>" : 
                "<<FUNCTION>>");
    adopt1(function, ident);
    adopt1(function, arglist);
    if(!prototype)
        adopt1(function, block);
    return function;
}


static void dump_node (FILE* outfile, astree* node) {
    const char *tname = get_yytname(node->symbol);
    if(strstr(tname, "TOK_") == tname)
        tname += 4;
   fprintf (outfile, "%s \"%s\" %ld.%ld.%ld {%d} %s",
           tname, node->lexinfo->c_str(), node->filenr,
           node->linenr, node->offset, node->blocknr,
           __typeid_attrs_string(get_node_attributes(node),
               node->type_name).c_str());

    if(node->symentry && node->symentry->definition != node)
        fprintf(outfile, " (%ld.%ld.%ld)",
                node->symentry->filenr,
                node->symentry->linenr, node->symentry->offset);
}

static void dump_astree_rec (FILE* outfile, astree* root,
                             int depth) {
   if (root == NULL) return;
   for(int i=0;i<depth;i++)
       fprintf(outfile, "|  ");
   dump_node (outfile, root);
   fprintf (outfile, "\n");
   for (size_t child = 0; child < root->children.size();
        ++child) {
      dump_astree_rec (outfile, root->children[child],
                       depth + 1);
   }
}

void dump_astree (FILE* outfile, astree* root) {
   dump_astree_rec (outfile, root, 0);
   fflush (NULL);
}

void free_ast (astree* root) {
   while (not root->children.empty()) {
      astree* child = root->children.back();
      root->children.pop_back();
      free_ast (child);
   }
   DEBUGF ('f', "free [%p]-> %d:%d.%d: %s: \"%s\")\n",
           root, root->filenr, root->linenr, root->offset,
           get_yytname (root->symbol), root->lexinfo->c_str());
   delete root;
}

void free_ast2 (astree* tree1, astree* tree2) {
   free_ast (tree1);
   free_ast (tree2);
}

