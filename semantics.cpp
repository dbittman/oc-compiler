#include "semantics.h"
#include "astree.h"
#include "lyutils.h"
#include <cassert>
#include <map>

using namespace std;

size_t print_depth=0;
int semantic_errors = 0;
int dfs_traverse(astree *node);
FILE *symfile = stdout;
const string *current_function = NULL;
const string *current_structure = NULL;

int process_node(astree *node)
{
    assert(node->symbol != TOK_DECLID);
    if(node->symbol == TOK_IDENT) {
        /* look up the symbol */
        symbol *sym = find_symbol(node->lexinfo);
        if(!sym) {
            fprintf(stderr, 
                    "%ld.%2ld.%3.3ld: identifier '%s' is undefined\n",
                    node->filenr, node->linenr, node->offset,
                    node->lexinfo->c_str());
            semantic_errors++;
        } else {
            node->symentry = sym;
            node->type_name = sym->type_name;
        }
    } else {
        if(!process_attributes(node))
            semantic_errors++;
    }
    return 0;
}

int handle_structure(astree *node)
{
    if(symbol_stack.size() != 1) {
        fprintf(stderr,
                "%ld.%2ld.%3.3ld: structures must be in global scope\n",
                node->filenr, node->linenr, node->offset);
        semantic_errors++;
        return 1;
    }
    /* check for existing typeid */
    symbol *sym = find_symbol_in_table(typeid_table,
            node->children[0]->lexinfo);
    if(sym) {
        fprintf(stderr, 
                "%ld.%2ld.%3.3ld: duplicate"
                "declaration of typeid '%s'\n",
                node->filenr, node->linenr, node->offset,
                node->children[0]->lexinfo->c_str());
        semantic_errors++;
        return 1;
    }
    fprintf(symfile, "%s (%ld.%ld.%ld) {0} struct \"%s\"\n",
            node->children[0]->lexinfo->c_str(),
            node->filenr, node->linenr, node->offset,
            node->children[0]->lexinfo->c_str());
    sym = create_symbol_in_table(typeid_table, node->children[0]);
    current_structure = node->children[0]->lexinfo;
    sym->block_nr = 0;
    node->children[0]->symentry = sym;
    node->children[0]->blocknr = 0;
    sym->attributes.set(ATTR_typeid);
    sym->block_nr = 0;

    print_depth++;

    symbol_table *field_table = new symbol_table();

    for(size_t child = 1; child < node->children.size(); ++child) {
        /* add each field to the field_table */
        symbol *sym =
            symbolize_declaration(field_table,
                    node->children[child],
                    attr_bitset(1 << ATTR_field));
        sym->type_name = node->children[0]->lexinfo;
        sym->block_nr = 0;
    }
    print_depth--;
    current_structure = 0;
    sym->fields = field_table;
    fprintf(symfile, "\n");
    return 0;
}

int handle_function(astree *node)
{
    if(scope_get_current_depth() != 0) {
        fprintf(stderr,
                "%ld.%2ld.%3.3ld: functions must be in global scope\n",
                node->filenr, node->linenr, node->offset);
        semantic_errors++;
        return 1;
    }
    if(node->children.size() == 2) {
        /* prototype */
        astree *decl;
        if(node->children[0]->symbol == TOK_ARRAY)
            decl = node->children[0]->children[1];
        else
            decl = node->children[0]->children[0];
        symbol *sym;
        if((sym = find_symbol_in_table(scope_get_global_table(),
                        decl->lexinfo))) {
            /* found a previous prototype */
            astree *prototype = sym->definition->parent->parent;
            if(!typecheck_compare_functions(prototype, node)) {
                fprintf(stderr,
                        "%ld.%2ld%3.3ld: function has mis-matching"
                        " prototype (declared at %ld.%2ld.%3.3ld)\n",
                        node->filenr, node->linenr, node->offset,
                        prototype->filenr, prototype->linenr,
                        prototype->offset);
                semantic_errors++;
                return 1;
            }
            return 0;
        }
    }
    symbol *sym =
        symbolize_declaration(scope_get_global_table(),
                node->children[0], attr_bitset(1 << ATTR_function));

    if(sym) {
        if(sym->definition->parent != node->children[0]) {
            /* found a previous prototype */
            astree *prototype = sym->definition->parent->parent;
            if(!typecheck_compare_functions(prototype, node)) {
                fprintf(stderr,
                        "%ld.%2ld%3.3ld: function has mis-matching"
                        " prototype (declared at %ld.%2ld.%3.3ld)\n",
                        node->filenr, node->linenr, node->offset,
                        prototype->filenr, prototype->linenr,
                        prototype->offset);
                semantic_errors++;
            }
        }
    } else {
        semantic_errors++;
        return 1;
    }
    current_function = sym->definition->lexinfo;
    sym->fnblock = 0;

    enter_block();
    print_depth++;
    astree *params = node->children[1];
    params->blocknr = get_current_block();
    /* in case we're re-processing params */ 
    sym->params.clear();
    for (size_t child = 0; child < params->children.size();
                ++child) {
        symbol *paramsym = symbolize_declaration(
                scope_get_top_table(),
                params->children[child],
                attr_bitset(1 << ATTR_param));
        sym->params.push_back(paramsym);
    }
    fprintf(symfile, "\n");

    if(node->symbol == TOK_FUNCTION) {
        /* manually parse the block */
        astree *block = node->children[2];
        block->blocknr = get_current_block();
        sym->fnblock = block;
        for (size_t child = 0; child < block->children.size();
                ++child) {
            dfs_traverse(block->children[child]);
        }
    }
    fprintf(symfile, "\n");
    leave_block();
    print_depth--;

    current_function = NULL;

    return 0;
}

int dfs_traverse(astree *node)
{
    switch(node->symbol) {
        case TOK_FUNCTION:case TOK_PROTOTYPE:
            handle_function(node);
            break;
        case TOK_STRUCT:
            handle_structure(node);
            break;
        case TOK_INT: case TOK_CHAR: case TOK_BOOL: case TOK_TYPEID:
        case TOK_STRING: case TOK_ARRAY:
            symbolize_declaration(scope_get_top_table(), node, 0);
            break;
        case TOK_VOID:
            fprintf(stderr,
                    "%ld.%2ld.%3.3ld: cannot have void variables\n",
                    node->filenr, node->linenr, node->offset);
            semantic_errors++;
            break;
        case TOK_NEW:
            process_node(node->children[0]);
            process_node(node);
            if(!node->type_name
                    || !find_symbol_in_table(typeid_table,
                        node->type_name)) {
                fprintf(stderr, 
                        "%ld.%2ld.%3.3ld: allocator with"
                        " unknown typeid '%s'\n",
                        node->filenr, node->linenr, node->offset,
                        node->type_name ?
                        node->type_name->c_str() : "???");
                semantic_errors++;
            }
            break;
        case TOK_NEWARRAY:
            dfs_traverse(node->children[1]);
            process_node(node->children[0]);
            break;
        case TOK_NEWSTRING:
            dfs_traverse(node->children[0]);
            break;
        case '.':
            /* look up everything */
            dfs_traverse(node->children[0]);
            process_node(node->children[1]);
            typeid_table_field_select(node);
            break;
        default:
            if(node->symbol == TOK_BLOCK) {
                print_depth++;
                enter_block();
            }
            for (size_t child = 0; child < node->children.size();
                    ++child) {
                dfs_traverse(node->children[child]);
            }
    }
    if(node->symbol != TOK_FUNCTION 
            && node->symbol != TOK_PROTOTYPE 
            && node->symbol != TOK_STRUCT
            && node->symbol != TOK_VOID) {
        process_node(node);
        node->blocknr = block_num_stack.back();
        if(node->symbol == TOK_BLOCK) {
            print_depth--;
            leave_block();
        }
    }
    return 0;
}

int oc_run_semantics(astree *root, FILE *file)
{
    symfile = file;
    symbol_stack.push_back(new symbol_table()); /* top-level symbols */
    block_num_stack.push_back(0);
    dfs_traverse(root);
    return semantic_errors;
}

