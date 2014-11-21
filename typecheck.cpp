#include <vector>
#include <bitset>
#include "type.h"
#include "semantics.h"
#include "astree.h"
#include <cassert>
#include "lyutils.h"
using namespace std;

const char *attr_names[ATTR_bitset_size] = {
    [ATTR_void]     = "void",
    [ATTR_bool]     = "bool",
    [ATTR_char]     = "char",
    [ATTR_int]      = "int",
    [ATTR_null]     = "null",
    [ATTR_string]   = "string",
    [ATTR_struct]   = "struct",
    [ATTR_array]    = "array",
    [ATTR_function] = "function",
    [ATTR_variable] = "variable",
    [ATTR_field]    = "field",
    [ATTR_typeid]   = "typeid",
    [ATTR_param]    = "param",
    [ATTR_lval]     = "lval",
    [ATTR_const]    = "const",
    [ATTR_vreg]     = "vreg",
    [ATTR_vaddr]    = "vaddr",
};

string attrs_string(attr_bitset attr)
{
    string ret = string("");
    for(int i=0;i<ATTR_bitset_size;i++) {
        if(attr.test(i)) {
            ret += string(attr_names[i]);
            ret += " ";
            if(i == ATTR_struct) {
                /* print struct name */
                /* TODO */
#warning "TODO"
                ret += "TODOSTRUCTNAME";
                ret += " ";
            }
        }
    }
    return ret;
}

unordered_map<int,int> tok_basetype_to_attr_map = {
    {TOK_INT, ATTR_int},
    {TOK_CHAR, ATTR_char},
    {TOK_BOOL, ATTR_bool},
    {TOK_TYPEID, ATTR_struct},
    {TOK_VOID, ATTR_void},
    {TOK_STRING, ATTR_string},
    {TOK_ARRAY, ATTR_array},
};

void node_generate_attributes(astree *node, attr_bitset &attr)
{
    auto it = tok_basetype_to_attr_map.find(node->symbol);
    assert(it != tok_basetype_to_attr_map.end());
    attr.set(it->second);
    if(node->symbol == TOK_ARRAY) {
        /* arrays have the basetype stored as the first child, so
         * we recurse there to get the full type of the node */
        node_generate_attributes(node->children[0], attr);
    }
}

attr_bitset get_node_attributes(astree *node)
{
    switch(node->symbol) {
        case TOK_IDENT:
        case TOK_FIELD:
        case TOK_DECLID:
            /* lookup */
            if(node->symentry)
                return node->symentry->attributes;
            return 0;
        default:
            return node->attributes;
    }
}

/* yeah, okay, #defines are evil, but this gets annoying to type a lot */
#define childattr(n) get_node_attributes(node->children[n])
#define childnode(n) (node->children[n])
#define BIT(x) attr_bitset(1 << x)

int attr_check_required(astree *node, attr_bitset required)
{
    /* check if node_attr has all the bits set by 'required' as well. */
    for(int i=0;i<ATTR_bitset_size;i++) {
        if(required.test(i) && !get_node_attributes(node).test(i)) {
            fprintf(stderr, "typecheck: %ld.%ld.%ld has {%s}, and {%s} is required\n",
                    node->filenr, node->linenr, node->offset,
                    attrs_string(get_node_attributes(node)).c_str(), attrs_string(required).c_str());
            return 0;
        }
    }
    return 1;
}

int attr_check_notallowed(astree *node, attr_bitset notallowed)
{
    /* check if node_attr has none of the bits set by 'notallowed' */
    for(int i=0;i<ATTR_bitset_size;i++) {
        if(notallowed.test(i) && get_node_attributes(node).test(i)) {
            fprintf(stderr, "typecheck: %ld.%ld.%ld has {%s}, but none of {%s} are allowed\n",
                    node->filenr, node->linenr, node->offset,
                    attrs_string(get_node_attributes(node)).c_str(), attrs_string(notallowed).c_str());
            return 0;
        }
    }
    return 1;
}

int attr_check_any(astree *node, attr_bitset sets)
{
    /* check if node_attr has at least one of the bits set by 'sets' */
    for(int i=0;i<ATTR_bitset_size;i++) {
        if(sets.test(i) && get_node_attributes(node).test(i)) {
            return 1;
        }
    }
    fprintf(stderr, "typecheck: %ld.%ld.%ld has {%s}, but at least one of {%s} are required\n",
            node->filenr, node->linenr, node->offset,
            attrs_string(get_node_attributes(node)).c_str(), attrs_string(sets).c_str());
    return 0;
}

int attr_handle_binop(astree *node)
{
    node->attributes.set(ATTR_int);
    node->attributes.set(ATTR_vreg);
    return attr_check_required(childnode(0), BIT(ATTR_int))
        && attr_check_required(childnode(1), BIT(ATTR_int))
        && attr_check_notallowed(childnode(0), BIT(ATTR_array))
        && attr_check_notallowed(childnode(1), BIT(ATTR_array));
}

int attr_handle_unop(astree *node)
{
    int childtype, proptype;
    switch(node->symbol) {
        case TOK_NEG: case TOK_POS:
            childtype = proptype = ATTR_int;
            break;
        case TOK_ORD:
            childtype = ATTR_char;
            proptype = ATTR_int;
            break;
        case TOK_CHR:
            childtype = ATTR_int;
            proptype = ATTR_char;
            break;
        case '!':
            childtype = proptype = ATTR_bool;
            break;
    }
    node->attributes.set(ATTR_vreg);
    node->attributes.set(proptype);
    return attr_check_required(childnode(0), BIT(childtype))
        && attr_check_notallowed(childnode(0), BIT(ATTR_array));
}

#define PRIMITIVE attr_bitset((1 << ATTR_int) | (1 << ATTR_char) | (1 << ATTR_bool))
#define REFERENCE attr_bitset((1 << ATTR_string) | (1 << ATTR_array) | (1 << ATTR_struct))
#define ANY (PRIMITIVE | REFERENCE)
#define BASE (PRIMITIVE | attr_bitset(1 << ATTR_struct) | attr_bitset(ATTR_string))

#warning "check for same typeid"

int attr_check_compatible(astree *node, attr_bitset a, attr_bitset b)
{
    if((a & ANY) == (b & ANY))
        return 1;
    if((a & REFERENCE).any() && (b.test(ATTR_null)))
        return 1;
    if((b & REFERENCE).any() && (a.test(ATTR_null)))
        return 1;
    fprintf(stderr, "typecheck: %ld.%ld.%ld: nodes are not compatible: have {%s} and {%s}\n",
            node->filenr, node->linenr, node->offset,
            attrs_string(a).c_str(), attrs_string(b).c_str());
    return 0;
}

int attr_handle_comparison(astree *node)
{
    node->attributes = BIT(ATTR_bool) | BIT(ATTR_vreg);
    if(node->symbol == TOK_EQ || node->symbol == TOK_NE) {
        return attr_check_compatible(node, childattr(0), childattr(1))
            && attr_check_any(childnode(0), ANY)
            && attr_check_any(childnode(1), ANY);
    } else {
        return attr_check_compatible(node, childattr(0), childattr(1))
            && attr_check_any(childnode(0), PRIMITIVE)
            && attr_check_any(childnode(1), PRIMITIVE);
    }
}

int attr_handle_new(astree *node)
{
    int res = 0;
    switch(node->symbol) {
        case TOK_NEW:
            /* the only want the AST is correct is if the attributes 
             * are correct, so we don't need to check */
            node->attributes = childattr(0) | BIT(ATTR_vreg);
            node->type_name = node->children[0]->type_name;
            res = 1;
            break;
        case TOK_NEWARRAY:
            node->attributes = (childattr(0) & BASE) | BIT(ATTR_array) | BIT(ATTR_vreg);

            res = attr_check_required(childnode(1), BIT(ATTR_int))
                && attr_check_notallowed(childnode(1), BIT(ATTR_array));
            break;
        case TOK_NEWSTRING:
            node->attributes = BIT(ATTR_string) | BIT(ATTR_vreg);
            res = attr_check_required(childnode(0), BIT(ATTR_int))
                && attr_check_notallowed(childnode(0), BIT(ATTR_array));
            break;
    }
    return res;
}

int attr_compare_params(astree *node, symbol *param)
{
    attr_bitset attr = get_node_attributes(node);
    return attr_check_compatible(node, attr, param->attributes)
        && attr_check_any(node, ANY)
        && attr_check_any(node, ANY);
}

int attr_handle_call(astree *node)
{
    symbol *func = childnode(0)->symentry;
    if(!func) {
        return 0;
    }
    int num_params = node->children.size() - 1;
    if(num_params != func->params.size()) {
        fprintf(stderr, "typecheck: %ld.%ld.%ld: invalid number of parameters to "
                "function '%s' (needed %ld, have %d)\n",
                node->filenr, node->linenr, node->offset, childnode(0)->lexinfo->c_str(),
                func->params.size(), num_params);
        return 0;
    }
    int fails = 0;
    for(int i = 0;i < num_params;i++) {
        if(!attr_compare_params(childnode(i + 1), func->params[i]))
            fails++;
    }
    return (fails == 0);
}

int attr_handle_constant(astree *node)
{
    int type;
    switch(node->symbol) {
        case TOK_FALSE: case TOK_TRUE:
            type = ATTR_bool;
            break;
        case TOK_STRINGCON:
            type = ATTR_string;
            break;
        case TOK_CHARCON:
            type = ATTR_char;
            break;
        case TOK_INTCON:
            type = ATTR_int;
            break;
        case TOK_NULL:
            type = ATTR_null;
            break;
    }
    node->attributes.set(type);
    node->attributes.set(ATTR_const);
    return 1;
}

int attr_handle_index(astree *node)
{
    if(!childattr(0).test(ATTR_array)) {
        node->attributes = BIT(ATTR_char) | BIT(ATTR_vaddr) | BIT(ATTR_lval);
        if(!childattr(0).test(ATTR_string)) {
            fprintf(stderr, "typecheck: %ld.%ld.%ld: cannot index into non-array non-string value\n",
                    childnode(0)->filenr, childnode(0)->linenr, childnode(0)->offset);
            return 0;
        }
        return 1;
    }
    node->attributes = BIT(ATTR_lval) | BIT(ATTR_vaddr) | (childattr(0) & BASE);
    return attr_check_required(childnode(1), BIT(ATTR_int))
        && attr_check_notallowed(childnode(1), BIT(ATTR_array))
        && attr_check_any(childnode(0), BASE);
}

int attr_handle_field_selector(astree *node)
{
    node->attributes = BIT(ATTR_vaddr) | BIT(ATTR_lval);
    node->attributes |= (childattr(1) & ANY);
    return 1;
}

int attr_handle_assignment(astree *node)
{
    node->attributes = (childattr(1) & ANY) | BIT(ATTR_vreg);
    return attr_check_required(childnode(0), BIT(ATTR_lval))
        && attr_check_compatible(node, childattr(0), childattr(1))
        && attr_check_any(childnode(0), ANY)
        && attr_check_any(childnode(1), ANY);

}

int attr_handle_conditional(astree *node)
{
    return attr_check_required(childnode(0), BIT(ATTR_bool))
        && attr_check_notallowed(childnode(0), BIT(ATTR_array));
}

int attr_handle_return(astree *node)
{
    symbol *func = NULL;
    if(current_function)
        func = find_symbol_in_table(scope_get_global_table(), current_function);
    assert(func || !current_function);
    if(node->symbol == TOK_RETURNVOID) {
        if(!func)
            return 1;
        if(!func->attributes.test(ATTR_void))
            fprintf(stderr, "typecheck: %ld.%ld.%ld: can't return void in a non-void function\n",
                    node->filenr, node->linenr, node->offset);
        return func->attributes.test(ATTR_void);
    }
    /* okay, do it with types this time */
    return attr_check_compatible(node, childattr(0), func->attributes)
        && attr_check_any(childnode(0), ANY);
}

int attr_handle_vardecl(astree *node)
{
    return attr_check_compatible(node, childattr(0), childattr(1))
        && attr_check_any(childnode(0), ANY)
        && attr_check_any(childnode(1), ANY)
        && attr_check_required(childnode(0), BIT(ATTR_lval));
}

int attr_handle_type(astree *node)
{
    if(!node->children.size()) {
        node_generate_attributes(node, node->attributes);
        if(node->symbol == TOK_TYPEID)
            node->type_name = node->lexinfo;
        return 1;
    }
    int childnr = 0;
    if(node->symbol == TOK_ARRAY)
        childnr = 1;
    if(node->symbol == TOK_TYPEID) {
        node->type_name = node->children[childnr]->type_name;
    }
    node->attributes = childattr(childnr);
    /* TODO ? */
    //node->symentry = node->children[childnr]->symentry;
    return 1;
}

int process_attributes(astree *node)
{
    int res;
    switch(node->symbol) {
        case '+': case '-': case '*': case '/': case '%':
            res = attr_handle_binop(node);
            break;
        case TOK_POS: case TOK_NEG: case '!': case TOK_ORD: case TOK_CHR:
            res = attr_handle_unop(node);
            break;
        case TOK_LE: case TOK_EQ: case TOK_GE: case TOK_NE: case '>': case '<':
            res = attr_handle_comparison(node);
            break;
        case TOK_NEW: case TOK_NEWARRAY: case TOK_NEWSTRING:
            res = attr_handle_new(node);
            break;
        case TOK_CALL:
            res = attr_handle_call(node);
            break;
        case TOK_INTCON: case TOK_STRINGCON: case TOK_CHARCON: case TOK_FALSE:
        case TOK_TRUE: case TOK_NULL:
            res = attr_handle_constant(node);
            break;
        case TOK_INDEX:
            res = attr_handle_index(node);
            break;
        case '.':
            res = attr_handle_field_selector(node);
            break;
        case '=':
            res = attr_handle_assignment(node);
            break;
        case TOK_WHILE: case TOK_IF: case TOK_IFELSE:
            res = attr_handle_conditional(node);
            break;
        case TOK_RETURN: case TOK_RETURNVOID:
            res = attr_handle_return(node);
            break;
        case TOK_VARDECL:
            res = attr_handle_vardecl(node);
            break;
        case TOK_INT: case TOK_CHAR: case TOK_STRING: case TOK_BOOL: case TOK_ARRAY: case TOK_TYPEID:
            res = attr_handle_type(node);
            break;
    }
    return res;
}

