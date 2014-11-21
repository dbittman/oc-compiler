#ifndef __SEMANTICS_H__
#define __SEMANTICS_H__

#include <string>
#include <unordered_map>
#include "oc.h"
#include "astree.h"
#include <vector>
#include "type.h"

struct symbol;

using symbol_table = unordered_map<const string*,symbol*>;
using symbol_entry = pair<const string*,symbol*>;

struct symbol {
    attr_bitset attributes;
    symbol_table *fields;
    size_t filenr, linenr, offset;
    size_t block_nr;
    vector<symbol *> params;
    astree *definition;
    struct symbol *type;
    const string *type_name;
};

#define SCOPE_GLOBAL 0

extern size_t next_block ;
extern vector<size_t> block_num_stack;
extern vector<symbol_table*> symbol_stack;
/* has function and struct definitions, along with global code statements */
extern symbol_table *typeid_table;
extern const string *current_function;
void node_generate_attributes(astree *node, attr_bitset &attr);

int oc_run_semantics(astree *root);
int scope_get_current_depth();
symbol_table *scope_get_global_table();
symbol_table *scope_get_current_table();
void enter_block();
void leave_block();
size_t get_current_block();
symbol_table *scope_get_top_table();
symbol *create_symbol_in_table(symbol_table *table, astree *node);
struct symbol *create_symbol(struct astree *node, attr_bitset attrs, const string *name);
struct symbol *find_symbol_in_table(symbol_table *table, const string *ident);
struct symbol *find_symbol(const string *ident);
symbol *symbolize_declaration(symbol_table *table, astree *node, attr_bitset initial_attr);

attr_bitset get_node_attributes(astree *node);
int process_attributes(astree *node);
int typeid_table_field_select(astree *node);
#endif

