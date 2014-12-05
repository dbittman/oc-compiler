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
        }
    }
    if(ret.back() == ' ')
        ret.pop_back();
    return ret;
}

string __typeid_attrs_string(attr_bitset attr, const string *type_name)
{
    string ret = string("");
    for(int i=0;i<ATTR_bitset_size;i++) {
        if(attr.test(i)) {
            ret += string(attr_names[i]);
            ret += " ";
            if(i == ATTR_struct) {
                ret += "\"";
                ret += *type_name;
                ret += "\" ";
            }
        }
    }
    if(ret.back() == ' ')
        ret.pop_back();
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

int node_generate_attributes(astree *node, attr_bitset &attr)
{
    auto it = tok_basetype_to_attr_map.find(node->symbol);
    assert(it != tok_basetype_to_attr_map.end());
    attr.set(it->second);
    if(node->symbol == TOK_ARRAY) {
        /* arrays have the basetype stored as the first child, so
         * we recurse there to get the full type of the node */
        if(node->children[0]->symbol == TOK_VOID) {
            fprintf(stderr,
                    "%ld.%2ld.%3.3ld: cannot have void arrays\n",
                    node->filenr, node->linenr, node->offset);
            return 0;
        }
        return node_generate_attributes(node->children[0], attr);
    }
    if(node->symbol == TOK_VOID && !attr.test(ATTR_function)) {
        fprintf(stderr,
                "%ld.%2ld.%3.3ld: cannot have void declarations\n",
                node->filenr, node->linenr, node->offset);
        return 0;
    }
    if(!attr.test(ATTR_function) && !attr.test(ATTR_field))
        attr.set(ATTR_variable);
    return 1;
}

attr_bitset get_node_attributes(astree *node)
{
    switch(node->symbol) {
        case TOK_IDENT:
        case TOK_FIELD:
        case TOK_DECLID:
        case TOK_TYPEID:
            /* lookup */
            if(node->symentry)
                return node->symentry->attributes;
        default:
            return node->attributes;
    }
}

/* yeah, okay, #defines are evil, but
 * this gets annoying to type a lot */
#define childattr(n) get_node_attributes(node->children[n])
#define childnode(n) (node->children[n])
#define BIT(x) attr_bitset(1 << x)

/* Okay. A lot of these typechecks come down to checking child nodes
 * against several restrictions:
 *     - A list of attributes that are not allowed
 *     - A list of attributes that are required
 *     - A list of attributes of which at least one must be set.
 * While there are several special cases, these cover a lot of cases.
 *
 * I have written a couple of functions that check these conditions,
 * given a node and a bitset, we can check allowed, required, and
 * disallowed. This also allows a centralized system to print out
 * (possibly cryptic at first) error messages.
 *
 * Additionally, a typecheck comes down to testing each of these
 * conditions multiple times against different bitsets. Because of
 * this, a macro CONDRETURN is defined, which simplifies this.
 * If any of these conditions return false, we want to return
 * false for the typecheck. Thus, we rely on a variable named
 * 'res' which is returned at the end of the function.
 *
 * Yes, I know that #defines are 'dangerous'. This is fine, and works.
 * And even Mackey uses them, so I can too!
 */

#define CONDRETURN(x) if(!x) res = 0

int attr_check_required(astree *node, attr_bitset required)
{
    /* check if node_attr has all the bits set by 'required' as well. */
    for(int i=0;i<ATTR_bitset_size;i++) {
        if(required.test(i) && !get_node_attributes(node).test(i)) {
            fprintf(stderr,
                    "%ld.%2ld.%3.3ld: node only has {%s},"
                    " and {%s} is required\n",
                    node->filenr, node->linenr, node->offset,
                    attrs_string(get_node_attributes(node)).c_str(),
                    attrs_string(required).c_str());
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
            fprintf(stderr,
                    "%ld.%2ld.%3.3ld: node has {%s}, but none"
                    " of {%s} are allowed\n",
                    node->filenr, node->linenr, node->offset,
                    attrs_string(get_node_attributes(node)).c_str(),
                    attrs_string(notallowed).c_str());
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
    fprintf(stderr, 
            "%ld.%2ld.%3.3ld: node has {%s}, but at least"
            " one of {%s} are required\n",
            node->filenr, node->linenr, node->offset,
            attrs_string(get_node_attributes(node)).c_str(),
            attrs_string(sets).c_str());
    return 0;
}

/* each of these functions handles a specific node. They
 * should be pretty self-explanitory. */

int attr_handle_binop(astree *node)
{
    node->attributes.set(ATTR_int);
    node->attributes.set(ATTR_vreg);
    int res = 1;
    CONDRETURN(attr_check_required(childnode(0), BIT(ATTR_int)));
    CONDRETURN(attr_check_required(childnode(1), BIT(ATTR_int)));
    CONDRETURN(attr_check_notallowed(childnode(0), BIT(ATTR_array)));
    CONDRETURN(attr_check_notallowed(childnode(1), BIT(ATTR_array)));
    return res;
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
    int res = 1;
    CONDRETURN(attr_check_required(childnode(0),
                BIT(childtype)));
    CONDRETURN(attr_check_notallowed(childnode(0),
                BIT(ATTR_array)));
    return res;
}

#define PRIMITIVE \
    attr_bitset((1 << ATTR_int) | (1 << ATTR_char) | \
            (1 << ATTR_bool))
#define REFERENCE \
    attr_bitset((1 << ATTR_string) | \
            (1 << ATTR_array) | (1 << ATTR_struct) | \
            (1 << ATTR_null))
#define ANY (PRIMITIVE | REFERENCE)
#define BASE \
    (PRIMITIVE | attr_bitset(1 << ATTR_struct) | \
     attr_bitset(1 << ATTR_string))

/* this does the "compatible" check as defined by the typecheck
 * grammar */
int attr_check_compatible(astree *node, attr_bitset a, attr_bitset b)
{
    if((a & ANY) == (b & ANY))
        return 1;
    if((a & REFERENCE).any() && (b.test(ATTR_null)))
        return 1;
    if((b & REFERENCE).any() && (a.test(ATTR_null)))
        return 1;
    if(a.any() && b.any()) {
        fprintf(stderr,
                "%ld.%2ld.%3.3ld: nodes are not compatible:"
                " have {%s} and {%s}\n",
                node->filenr, node->linenr, node->offset,
                attrs_string(a).c_str(), attrs_string(b).c_str());
    }
    return 0;
}

int attr_handle_comparison(astree *node)
{
    node->attributes = BIT(ATTR_bool) | BIT(ATTR_vreg);
    int res = 1;
    if(node->symbol == TOK_EQ || node->symbol == TOK_NE) {
        CONDRETURN(attr_check_compatible(node, childattr(0),
                    childattr(1)));
        CONDRETURN(attr_check_any(childnode(0), ANY));
        CONDRETURN(attr_check_any(childnode(1), ANY));
    } else {
        CONDRETURN(attr_check_compatible(node, childattr(0),
                    childattr(1)));
        CONDRETURN(attr_check_any(childnode(0), PRIMITIVE));
        CONDRETURN(attr_check_any(childnode(1), PRIMITIVE));
        CONDRETURN(attr_check_notallowed(childnode(1),
                    BIT(ATTR_array)));
        CONDRETURN(attr_check_notallowed(childnode(0),
                    BIT(ATTR_array)));
    }
    return res;
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
            node->attributes = (childattr(0) & BASE) 
                | BIT(ATTR_array) | BIT(ATTR_vreg);
            node->type_name = childnode(0)->type_name;

            res = attr_check_required(childnode(1), BIT(ATTR_int))
                && attr_check_notallowed(childnode(1),
                        BIT(ATTR_array));
            break;
        case TOK_NEWSTRING:
            node->attributes = BIT(ATTR_string) | BIT(ATTR_vreg);
            res = attr_check_required(childnode(0), BIT(ATTR_int))
                && attr_check_notallowed(childnode(0),
                        BIT(ATTR_array));
            break;
    }
    return res;
}

int attr_compare_params(astree *node, symbol *param)
{
    attr_bitset attr = get_node_attributes(node);
    int res = 1;
    CONDRETURN(attr_check_compatible(node, attr,
                param->attributes));
    CONDRETURN(attr_check_any(node, ANY));
    CONDRETURN(attr_check_any(node, ANY));
    return res;
}

int attr_handle_call(astree *node)
{
    symbol *func = childnode(0)->symentry;
    if(!func) {
        return 0;
    }
    /* check parameters */
    unsigned int num_params = node->children.size() - 1;
    if(num_params != func->params.size()) {
        fprintf(stderr,
                "%ld.%2ld.%3.3ld: invalid number of parameters to "
                "function '%s' (needed %ld, have %d)\n",
                node->filenr, node->linenr, node->offset,
                childnode(0)->lexinfo->c_str(),
                func->params.size(), num_params);
        return 0;
    }
    int fails = 0;
    for(unsigned i = 0;i < num_params;i++) {
        if(!attr_compare_params(childnode(i + 1), func->params[i]))
            fails++;
    }
    node->attributes = (func->attributes 
            | BIT(ATTR_vreg)) & ~(BIT(ATTR_function));
    node->type_name = func->type_name;
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
        node->attributes = BIT(ATTR_char) 
            | BIT(ATTR_vaddr) | BIT(ATTR_lval);
        if(!childattr(0).test(ATTR_string)) {
            if(childattr(0).any()) {
                fprintf(stderr,
                        "%ld.%2ld.%3.3ld: cannot index into"
                        " non-array non-string value\n",
                        childnode(0)->filenr, childnode(0)->linenr,
                        childnode(0)->offset);
            } else {
                node->attributes = BIT(ATTR_vaddr) | BIT(ATTR_lval);
            }
            return 0;
        }
        return 1;
    }
    node->attributes = BIT(ATTR_lval) | BIT(ATTR_vaddr) 
        | (childattr(0) & BASE);
    node->type_name = childnode(0)->type_name;
    int res = 1;
    CONDRETURN(attr_check_required(childnode(1), BIT(ATTR_int)));
    CONDRETURN(attr_check_notallowed(childnode(1), BIT(ATTR_array)));
    CONDRETURN(attr_check_any(childnode(0), BASE));
    return res;
}

int attr_handle_field_selector(astree *node)
{
    node->attributes = BIT(ATTR_vaddr) | BIT(ATTR_lval);
    node->attributes |= (childattr(1) & ANY);
    node->type_name = childnode(1)->type_name;
    int res = 1;
    CONDRETURN(attr_check_required(childnode(0), BIT(ATTR_struct)));
    CONDRETURN(attr_check_required(childnode(1), BIT(ATTR_field)));
    return res;
}

int attr_handle_assignment(astree *node)
{
    node->attributes = (childattr(1) & ANY) | BIT(ATTR_vreg);
    node->type_name = childnode(1)->type_name;
    int res = 1;
    CONDRETURN(attr_check_required(childnode(0), BIT(ATTR_lval)));
    CONDRETURN(attr_check_compatible(node, childattr(0), childattr(1)));
    CONDRETURN(attr_check_any(childnode(0), ANY));
    CONDRETURN(attr_check_any(childnode(1), ANY));
    return res;
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
        func = find_symbol_in_table(scope_get_global_table(),
                current_function);
    assert(func || !current_function);
    if(node->symbol == TOK_RETURNVOID) {
        if(!func)
            return 1;
        if(!func->attributes.test(ATTR_void))
            fprintf(stderr, "%ld.%2ld.%3.3ld: can't return"
                    " void in a non-void function\n",
                    node->filenr, node->linenr, node->offset);
        return func->attributes.test(ATTR_void);
    }
    /* okay, do it with types this time */
    if(!func) {
        fprintf(stderr, "%ld.%2ld.%3.3ld: can't return non-void"
                " in a void function (global scope)\n",
                node->filenr, node->linenr, node->offset);
        return 0;
    }
    return attr_check_compatible(node, childattr(0), func->attributes)
        && attr_check_any(childnode(0), ANY);
}

int attr_handle_vardecl(astree *node)
{
    int res = 1;
    CONDRETURN(attr_check_compatible(node, childattr(0),
                childattr(1)));
    CONDRETURN(attr_check_any(childnode(0), ANY));
    CONDRETURN(attr_check_any(childnode(1), ANY));
    CONDRETURN(attr_check_required(childnode(0), BIT(ATTR_lval)));
    return res;
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
    node->type_name = childnode(childnr)->type_name;
    return 1;
}

static int __fn_compare_nodes(astree *n1, astree *n2)
{
    if(n1->symbol != n2->symbol)
        return 0;
    if(n1->symbol == TOK_ARRAY) {
        if(n1->children[0]->symbol != n2->children[0]->symbol)
            return 0;
    }
    return 1;
}

int typecheck_compare_functions(astree *f1, astree *f2)
{
    if(!__fn_compare_nodes(f1->children[0], f2->children[0]))
        return 0;
    if(f1->children[1]->children.size() 
            != f2->children[1]->children.size())
        return 0;
    for(size_t i=0;i<f1->children[1]->children.size();i++) {
        if(!__fn_compare_nodes(f1->children[1]->children[i],
                    f2->children[1]->children[i]))
            return 0;
    }
    return 1;
}

int process_attributes(astree *node)
{
    int res = 1;
    switch(node->symbol) {
        case '+': case '-': case '*':
        case '/': case '%':
            res = attr_handle_binop(node);
            break;
        case TOK_POS: case TOK_NEG: case '!':
        case TOK_ORD: case TOK_CHR:
            res = attr_handle_unop(node);
            break;
        case TOK_LE: case TOK_EQ: case TOK_GE:
        case TOK_NE: case '>': case '<':
            res = attr_handle_comparison(node);
            break;
        case TOK_NEW: case TOK_NEWARRAY: case TOK_NEWSTRING:
            res = attr_handle_new(node);
            break;
        case TOK_CALL:
            res = attr_handle_call(node);
            break;
        case TOK_INTCON: case TOK_STRINGCON:
        case TOK_CHARCON: case TOK_FALSE:
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
        case TOK_INT: case TOK_CHAR: case TOK_STRING:
        case TOK_BOOL: case TOK_ARRAY: case TOK_TYPEID:
            res = attr_handle_type(node);
            break;
    }
    return res;
}

