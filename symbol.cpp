#include <vector>

#include "type.h"
#include "astree.h"
#include "lyutils.h"
#include "semantics.h"
using namespace std;

extern int semantic_errors;
size_t next_block = 1;
vector<size_t> block_num_stack;
vector<symbol_table*> symbol_stack;
/* has function and struct definitions,
 * along with global code statements */
symbol_table *typeid_table = new symbol_table();

int scope_get_current_depth()
{
    return symbol_stack.size() - 1;
}

symbol_table *scope_get_global_table()
{
    return symbol_stack[SCOPE_GLOBAL];
}

symbol_table *scope_get_current_table()
{
    return symbol_stack.back();
}

void enter_block()
{
    symbol_stack.push_back(NULL);
    block_num_stack.push_back(next_block);
    ++next_block;
}

void leave_block()
{
    symbol_stack.pop_back();
    block_num_stack.pop_back();
}

size_t get_current_block()
{
    return block_num_stack.back();
}

symbol_table *scope_get_top_table()
{
    if(symbol_stack.back() == NULL) {
        symbol_stack.back() = new symbol_table();
    }
    return symbol_stack.back();
}

symbol *create_symbol_in_table(symbol_table *table, astree *node)
{
    symbol *sym = new symbol();
    sym->filenr = node->filenr;
    sym->linenr = node->linenr;
    sym->offset = node->offset;
    sym->definition = node;
    node->symentry = sym;
    if(table) {
        symbol_entry entry(node->lexinfo, sym);
        table->insert(entry);
    }
    return sym;
}

struct symbol *find_symbol_in_table(symbol_table *table,
        const string *ident)
{
    if(table == NULL || ident == NULL)
        return NULL;
    auto entry = table->find(ident);
    if(entry == table->end())
        return NULL;
    return &*entry->second;
}

struct symbol *find_symbol(const string *ident)
{
    /* iterate backwards */
    for (unsigned i = symbol_stack.size(); i-- > 0;)
    {
        symbol_table *table = symbol_stack[i];
        if(table == NULL) {
            continue;
        }
        auto entry = table->find(ident);
        if(entry == table->end()) {
            continue;
        }
        return &*entry->second;
    }
    return NULL;
}

int typeid_table_field_select(astree *node)
{
    symbol *sym = find_symbol_in_table(typeid_table,
            node->children[0]->type_name);
    if(!sym) {
        fprintf(stderr,
                "%ld.%2ld.%3.3ld: typeid '%s' is undefined\n",
                node->filenr, node->linenr, node->offset,
                node->children[0]->type_name ?
                    node->children[0]->type_name->c_str()
                    : "???");
        semantic_errors++;
        return 1;
    }
    symbol *field = find_symbol_in_table(sym->fields,
            node->children[1]->lexinfo);
    if(!field) {
        fprintf(stderr,
                "%ld.%2ld.%3.3ld: typeid '%s' does not"
                " have a field '%s'\n",
                node->filenr, node->linenr, node->offset,
                node->children[0]->type_name->c_str(),
                node->children[1]->lexinfo->c_str());
        semantic_errors++;
        return 1;
    }
    node->symentry = field;
    node->children[1]->symentry = field;
    node->children[1]->type_name = field->type_name;
    return 0;
}

symbol *symbolize_declaration(symbol_table *table, astree *node,
        attr_bitset initial_attr)
{
    /* this has several possible things:
     * {BASETYPE}
     *     {DECLID}
     * {ARRAY}
     *     {BASETYPE}
     *     {DECLID}
     * {BASETYPE}
     *     {FIELD}
     * The goal is to input it into the symbol table correctly,
     * and check for duplicates and what-not. */
    attr_bitset attr = initial_attr;
    if(!node_generate_attributes(node, attr)) {
        semantic_errors++;
        return 0;
    }
    astree *decl;
    if(node->symbol == TOK_ARRAY)
        decl = node->children[1];
    else
        decl = node->children[0];
    symbol *prev_sym;
    if((prev_sym = find_symbol_in_table(table, decl->lexinfo))) {
        if(initial_attr.test(ATTR_function) && !prev_sym->fnblock) {
           return prev_sym; 
        } else {
            fprintf(stderr,
                    "%ld.%2ld.%3.3ld: duplicate declaration of"
                    " identifier '%s'. Previous"
                    " declaration at %ld.%ld.%ld\n",
                    node->filenr, node->linenr, node->offset,
                    decl->lexinfo->c_str(), prev_sym->filenr,
                    prev_sym->linenr, prev_sym->offset);
            semantic_errors++;
            return 0;
        }
    }
    struct symbol *sym = create_symbol_in_table(table, decl);
    if(!attr.test(ATTR_function) && !attr.test(ATTR_field))
        attr.set(ATTR_lval);
    /* additionally, if we're declaring a struct, grab the typeid */
    sym->type_name = NULL;
    if(attr.test(ATTR_struct)) {
        sym->type_name = node->lexinfo;
        decl->type_name = node->lexinfo;
    }
    sym->attributes = attr;
    sym->block_nr = get_current_block();
    decl->blocknr = get_current_block();
    node->blocknr = get_current_block();
    fprintf(symfile, "%*s%s (%ld.%ld.%ld)", (int)print_depth * 3, "",
            decl->lexinfo->c_str(), decl->filenr,
            decl->linenr, decl->offset);

    if(attr.test(ATTR_field)) {
        fprintf(symfile, " field {%s} ", current_structure->c_str());
    } else {
        fprintf(symfile, " {%ld} ", sym->block_nr);
    }
    fprintf(symfile, "%s\n", type_attrs_string(sym).c_str());

    return sym;
}

