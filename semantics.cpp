#include "semantics.h"
#include "astree.h"
#include "lyutils.h"
#include <cassert>
#include <map>

using namespace std;

size_t print_depth=0;
int semantic_errors = 0;
int dfs_traverse(astree *node);

const string *current_function = NULL;

int process_node(astree *node)
{
    assert(node->symbol != TOK_DECLID);
    if(node->symbol == TOK_IDENT) {
        /* look up the symbol */
        symbol *sym = find_symbol(node->lexinfo);
        if(!sym)
            fprintf(stderr, "[!!] symbol not found\n");
        node->symentry = sym;
        node->type_name = sym->type_name;
    } else {
        process_attributes(node);
    }
    return 0;
}

int handle_structure(astree *node)
{
    if(symbol_stack.size() != 1) {
        fprintf(stderr, "[!!] cannot have structure definition not in global scope\n");
        return 1;
    }

    /* check for existing typeid */
    symbol *sym = find_symbol_in_table(typeid_table, node->children[0]->lexinfo);
    if(sym) {
        fprintf(stderr, "[!!]: redefinition of struct\n");
        return 1;
    }

    sym = create_symbol_in_table(typeid_table, node->children[0]);
    sym->block_nr = 0;
    node->children[0]->symentry = sym;
    node->children[0]->blocknr = 0;
    sym->attributes.set(ATTR_typeid);
    sym->block_nr = 0;
    
    print_depth++;

    symbol_table *field_table = new symbol_table();

    for(size_t child = 1; child < node->children.size(); ++child) {
        /* add each field to the field_table */
        symbol *sym = symbolize_declaration(field_table, node->children[child], attr_bitset(1 << ATTR_field));
        sym->block_nr = 0;
    }
    sym->fields = field_table;
    return 0;
}

int handle_function(astree *node)
{
    if(scope_get_current_depth() != 0) {
        fprintf(stderr, "[!!] cannot have function definition not in global scope\n");
        return 1;
    }

    symbol *sym = symbolize_declaration(scope_get_global_table(), node->children[0], 
            attr_bitset(1 << ATTR_function));
    if(!sym) {
        fprintf(stderr, "COULDN'T CREAT FUNCTION\n");
        return 1;
    }
    
    current_function = sym->definition->lexinfo;

    enter_block();
    print_depth++;
    astree *params = node->children[1];
    for (size_t child = 0; child < params->children.size(); ++child) {
        symbol *paramsym = symbolize_declaration(scope_get_top_table(), params->children[child],
                attr_bitset(1 << ATTR_param));
        sym->params.push_back(paramsym);
    }

    int err = 0;
    if(node->symbol == TOK_FUNCTION) {
        /* manually parse the block */
        astree *block = node->children[2];
        for (size_t child = 0; child < block->children.size(); ++child) {
            dfs_traverse(block->children[child]);
        }
    }
    leave_block();
    
    current_function = NULL;

    return err;
}

#warning "handle all cases where type_name is a nullptr"

int dfs_traverse(astree *node)
{
    int errors = 0;
    switch(node->symbol) {
        case TOK_FUNCTION:case TOK_PROTOTYPE:
            if(handle_function(node))
                semantic_errors++;
            break;
        case TOK_STRUCT:
            if(handle_structure(node))
                semantic_errors++;
            break;
        case TOK_INT: case TOK_CHAR: case TOK_BOOL: case TOK_TYPEID:
        case TOK_STRING: case TOK_VOID: case TOK_ARRAY:
            if(!symbolize_declaration(scope_get_top_table(), node, 0))
                semantic_errors++;
            break;
        case TOK_NEW:
            process_node(node->children[0]);
            process_node(node);
            if(!node->type_name || !find_symbol_in_table(typeid_table, node->type_name))
                fprintf(stderr, "[!!] type not defined\n");
            break;
        case TOK_NEWARRAY:
            process_node(node->children[1]);
            /* fall through */
        case TOK_NEWSTRING:
            process_node(node->children[0]);
            break;
        case '.':
            /* look up everything */
            process_node(node->children[0]);
            process_node(node->children[1]);
            typeid_table_field_select(node);
            break;
        default:
            if(node->symbol == TOK_BLOCK) {
                print_depth++;
                enter_block();
            }
            for (size_t child = 0; child < node->children.size(); ++child) {
                if(dfs_traverse(node->children[child])) {
                    semantic_errors++;
                    errors++;
                }
            }
    }
    if(node->symbol != TOK_FUNCTION && node->symbol != TOK_PROTOTYPE && node->symbol != TOK_STRUCT) {
    process_node(node);
    node->blocknr = block_num_stack.back();
    if(node->symbol == TOK_BLOCK) {
        print_depth--;
        leave_block();
    }
    }
    return errors;
}

int oc_run_semantics(astree *root)
{
    symbol_stack.push_back(new symbol_table()); /* top-level symbols */
    block_num_stack.push_back(0);
    dfs_traverse(root);
    return 0;
}

