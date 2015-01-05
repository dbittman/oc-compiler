#include <string>
#include <vector>
#include <cstdio>
#include <cassert>
#include <cstring>

#include "semantics.h"
#include "astree.h"
#include "lyutils.h"
using namespace std;
FILE *oilfile;

size_t reg_nr = 1, str_nr = 1;
/* use C++'s auto-magic string concating to make more readable code */
#define INDENT "        "
/* this contains all the strings discovered at parse-time. */
vector<const string *> globalstrings;

/* DESIGN:
 * In general, a DFS traversal is done, post order. Each node may emit
 * some code, and each node may have a "result". If it has a result,
 * then the name used to refer to the result (whether it's a virtual
 * register or an identifier) is stored in node->oilname. Because the
 * traversal is done post order, when a node is processed, all of its
 * children have their oilname entry set. Thus, they can be used in
 * code emission!
 *
 * If a node emits code (like the '+' node), it uses the oilname of
 * its children. That way, if one of its children is another
 * operator, then it'll refer to the correct virtual register.
 *
 * If a node doesn't emit code (like the IDENT node), it just sets
 * the oilname correctly and returns.
 *
 * A few nodes are NOT done post-order! For the most part, a node's
 * children must be emitted first, but that's not always the case.
 * IF, IFELSE, and WHILE are the main ones. FUNCTION and STRUCT
 * are emitted entirely sperately, at the beginning of the emission
 * code.
 */

/* allocate a register. All registers share an indexing system */
static string *register_alloc(const char *type)
{
    return new string(string(type) + to_string(reg_nr++));
}

/* this is called from the parser. It stores all STRONGCONs in order
 * to emit all strings at the top of the file */
void emitter_register_string(astree *node)
{
    node->oilname = new string(string("s") + to_string(str_nr++));
    globalstrings.push_back(node->lexinfo);
}

string *strip_zeros(const string *lexstr)
{
    /* create a new string because lexinfo is a const string */
    string *stripped = new string(*lexstr);
    stripped->erase(0, stripped->find_first_not_of('0'));
    if(*stripped == string(""))
        *stripped += string("0");
    return stripped;
}

int test_is_operand(astree *node)
{
    return node->symbol == TOK_IDENT
        || node->symbol == TOK_INTCON
        || node->symbol == TOK_CHARCON;
}

/* register categories */
#define INT  0
#define BOOL 1
#define CHAR 2
#define PTR  3
static const char *rcategory[4] = {
    "i", "b", "c", "p"
};

/* this creates a string that can be used as a C type. The type is
 * calculated from the node's tokid and attributes. */
const char *get_result_type_name(astree *node)
{
    attr_bitset attr = get_node_attributes(node);
    if(attr.test(ATTR_struct))
        assert(node->type_name);
    const char *base;
    if(attr.test(ATTR_bool))
        base = "char";
    else if(attr.test(ATTR_char))
        base = "char";
    else if(attr.test(ATTR_int))
        base = "int";
    else if(attr.test(ATTR_string))
        base = "char*";
    else if(attr.test(ATTR_struct))
        base = "struct ";
    else
        assert(0);

    string *str = new string(string(base) +
            (attr.test(ATTR_struct) ? 
                string("s_") + *node->type_name 
                + string("*") : string("")) +
            (attr.test(ATTR_array) ? string("*") : string("")) + 
            (node->symbol == '.' ? string("*") : string("")));
    return str->c_str();
}

/* figure out the register category. This is mostly based on the tokid
 * except for TOK_CALL. */
const char *register_category(astree *node)
{
    const char *cat = NULL;
    switch(node->symbol) {
        case '+': case '-': case '*': case '/':
        case '%': case TOK_POS: case TOK_NEG:
        case TOK_ORD:
            cat = rcategory[INT];
            break;
        case '>': case '<': case TOK_EQ:
        case TOK_NE: case TOK_LE: case TOK_GE: 
        case '!':
            cat = rcategory[BOOL];
            break;
        case TOK_CHR:
            cat = rcategory[CHAR];
            break;
        case TOK_CALL:
            const char *result_type = get_result_type_name(node);
            /* is it a pointer? */
            if(strchr(result_type, '*'))
                cat = rcategory[PTR];
            else if(get_node_attributes(node).test(ATTR_int))
                cat = rcategory[INT];
            else if(get_node_attributes(node).test(ATTR_char))
                cat = rcategory[CHAR];
            else if(get_node_attributes(node).test(ATTR_bool))
                cat = rcategory[BOOL];
            else
                assert(0);
            break;
    }
    return cat;
}

string *mangle_name(astree *node)
{
    assert(node->symentry);
    if(node->symbol == TOK_FIELD) {
        return new string(string("f_") +
                /* okay, this is kinda ugly. Basically:
                 * node->symentry->definition is the
                 * field AST node that defines the field
                 * in the structure block. It's parent's
                 * parent is the actual 'struct' AST node,
                 * whose 0th child is the typename. */
                *node->symentry->definition->parent->
                    parent->children[0]->lexinfo + 
                string("_") + 
                *node->symentry->definition->lexinfo);
    }
    /* is global variable */
    if(node->symentry->block_nr == 0)
        return new string(string("__") + 
                *node->symentry->definition->lexinfo);
    else
        return new string(string("_") + 
                to_string(node->symentry->block_nr) + 
                string("_") + 
                *node->symentry->definition->lexinfo);
}

/* because not every node emits code, we can actually run this
 * function on sub-trees with the expectation that it does it
 * correctly. For example, we can run this on struct nodes, and
 * its children wont be printed out, but will have their oilname's
 * set. */
void emit_recursive(astree *node)
{
    /* these nodes must be done first, and they do NOT recurse on
     * its children automatically. This is because they need to
     * be traversed in a specific order and at specific times. */
    switch(node->symbol) {
        case TOK_STRUCT: case TOK_FUNCTION:
        case TOK_PROTOTYPE: case TOK_STRINGCON:
            return;
        case TOK_WHILE:
            fprintf(oilfile, "while_%ld_%ld_%ld:;\n",
                    node->filenr, node->linenr, node->offset);
            emit_recursive(node->children[0]);
            fprintf(oilfile, INDENT 
                    "if (!%s) goto break_%ld_%ld_%ld;\n",
                    node->children[0]->oilname->c_str(),
                    node->filenr, node->linenr, node->offset);
            emit_recursive(node->children[1]);
            fprintf(oilfile, INDENT "goto while_%ld_%ld_%ld;\n",
                    node->filenr, node->linenr, node->offset);
            fprintf(oilfile, "break_%ld_%ld_%ld:;\n",
                    node->filenr, node->linenr, node->offset);
            return;
        case TOK_IF:
            emit_recursive(node->children[0]);
            fprintf(oilfile, INDENT "if (!%s) goto fi_%ld_%ld_%ld;\n",
                    node->children[0]->oilname->c_str(),
                    node->filenr, node->linenr, node->offset);
            emit_recursive(node->children[1]);
            fprintf(oilfile, "fi_%ld_%ld_%ld:;\n",
                    node->filenr, node->linenr, node->offset);
            return;
        case TOK_IFELSE:
            emit_recursive(node->children[0]);
            fprintf(oilfile, INDENT "if (!%s) goto else_%ld_%ld_%ld;\n",
                    node->children[0]->oilname->c_str(),
                    node->filenr, node->linenr, node->offset);
            emit_recursive(node->children[1]);
            fprintf(oilfile, INDENT "goto fi_%ld_%ld_%ld;\n",
                    node->filenr, node->linenr, node->offset);
            fprintf(oilfile, "else_%ld_%ld_%ld:;\n",
                    node->filenr, node->linenr, node->offset);
            emit_recursive(node->children[2]);
            fprintf(oilfile, "fi_%ld_%ld_%ld:;\n",
                    node->filenr, node->linenr, node->offset);
            return;
    }
    /* post order */
    for(size_t child = 0;child < node->children.size();++child) {
        emit_recursive(node->children[child]);
    }
    string *reg;
    const char *sym;
    switch(node->symbol) {
        /* binary operators print out the operation and
         * store the result */
        case '+': case '-': case '*': case '>':
        case '/': case '%': case '<':
        case TOK_EQ: case TOK_NE: case TOK_LE:
        case TOK_GE: 
            node->oilname = register_alloc(register_category(node));
            fprintf(oilfile, INDENT "%s %s = %s %s %s;\n",
                    get_result_type_name(node),
                    node->oilname->c_str(), 
                    node->children[0]->oilname->c_str(), 
                    node->lexinfo->c_str(),
                    node->children[1]->oilname->c_str());
            
            break;
        /* so do unary operators */
        case TOK_POS: case TOK_NEG: case '!':
        case TOK_ORD: case TOK_CHR:
            node->oilname = register_alloc(register_category(node));
            if(node->symbol == TOK_ORD)
                sym = "(int)";
            else if(node->symbol == TOK_CHR)
                sym = "(char)";
            else
                sym = node->lexinfo->c_str();
            
            fprintf(oilfile, INDENT "%s %s = %s%s;\n",
                    get_result_type_name(node),
                    node->oilname->c_str(), sym,
                    node->children[0]->oilname->c_str());
            break;
        /* this is a special binary operator. The result is
         * just the lval, it can be used later */
        case '=':
            node->oilname = node->children[0]->oilname;
            fprintf(oilfile, INDENT "%s = %s;\n",
                    node->children[0]->oilname->c_str(),
                    node->children[1]->oilname->c_str());
            break;
        case TOK_VARDECL:
            fprintf(oilfile, INDENT);
            /* if we're a direct child of the root, then we're a
             * global variable and have already been declared
             * (see emit_globals). Skip the type part. */
            if(node->parent->symbol == TOK_ROOT) {
                if(node->children[0]->symbol == TOK_ARRAY)
                    fprintf(oilfile, "%s ", 
                            node->children[0]->
                            children[1]->oilname->c_str());
                else
                    fprintf(oilfile, "%s ",
                            node->children[0]->
                            children[0]->oilname->c_str());
            } else {
                fprintf(oilfile, "%s ", 
                        node->children[0]->oilname->c_str());
            }
            fprintf(oilfile, "= %s;\n", 
                    node->children[1]->oilname->c_str());
            break;
        case TOK_CALL:
            /* no register allocated on void function call */
            if(!get_node_attributes(node).test(ATTR_void)) {
                node->oilname = register_alloc(register_category(node));
                fprintf(oilfile, INDENT "%s %s = ",
                        get_result_type_name(node),
                        node->oilname->c_str());
            } else {
                fprintf(oilfile, INDENT);
            }
            fprintf(oilfile, "__%s (",
                    node->children[0]->lexinfo->c_str());
            /* emit arguments */
            for(size_t child = 1;child < node->children.size();
                    child++) {
                if(child != 1)
                    fprintf(oilfile, ", ");
                fprintf(oilfile, "%s", 
                        node->children[child]->oilname->c_str());
            }
            fprintf(oilfile, ");\n");
            break;
        case TOK_INTCON:
            node->oilname = strip_zeros(node->lexinfo);
            break;
        case TOK_CHARCON:
            node->oilname = node->lexinfo;
            break;
        case TOK_RETURN:
            fprintf(oilfile, INDENT "return %s;\n", 
                    node->children[0]->oilname->c_str());
            break;
        case TOK_RETURNVOID:
            fprintf(oilfile, INDENT "return;\n");
            break;
        case TOK_ARRAY:
            /* this is a declaration node,
             * just a little special name processing */
            node->oilname = new string(*node->children[0]->oilname 
                    + string("* ") + *node->children[1]->oilname);
            break;
        case TOK_INDEX:
            reg = register_alloc("a");
            fprintf(oilfile, INDENT "%s* %s = &%s[%s];\n",
                    get_result_type_name(node),
                    reg->c_str(),
                    node->children[0]->oilname->c_str(),
                    node->children[1]->oilname->c_str());

            node->oilname = new string(string("(*") 
                    + *reg + string(")")); 
            break;
        case '.':
            reg = register_alloc("a");
            fprintf(oilfile, INDENT "%s %s = &%s->%s;\n",
                    get_result_type_name(node),
                    reg->c_str(), node->children[0]->oilname->c_str(),
                    node->children[1]->oilname->c_str());

            node->oilname = new string(string("(*") + *reg +
                    string(")")); 
            break;
        case TOK_IDENT: case TOK_DECLID: case TOK_FIELD:
            node->oilname = mangle_name(node);
            break;
        /* for these type nodes, if they don't have children then
         * they're part of an array. So we let the array node handle
         * the naming. Otherwise, we just setup the name ourselves,
         * since that only depends on the children */
        case TOK_INT: case TOK_CHAR: case TOK_VOID:
            if(node->children.size() == 0)
                node->oilname = node->lexinfo;
            else
                node->oilname = new string(*node->lexinfo 
                        + " " + *node->children[0]->oilname);
            break;
        case TOK_BOOL:
            if(node->children.size() == 0)
                node->oilname = new string("char");
            else
                node->oilname = new string("char " 
                        + *node->children[0]->oilname);
            break;
        case TOK_STRING:
            if(node->children.size() == 0)
                node->oilname = new string("char*");
            else
                node->oilname = new string("char* " 
                        + *node->children[0]->oilname);
            break;
        case TOK_TYPEID:
            if(node->children.size() == 0)
                node->oilname = new string(string("struct s_") 
                        + *node->lexinfo + "*");
            else
                node->oilname = new string(string("struct s_") 
                        + *node->lexinfo + "* " 
                        + *node->children[0]->oilname);
            break;
        case TOK_NEW:
            reg = register_alloc("p");
            fprintf(oilfile, INDENT "struct s_%s* %s = xcalloc "
                    "(1, sizeof (struct s_%s));\n",
                    node->type_name->c_str(),
                    reg->c_str(),
                    node->type_name->c_str());
            node->oilname = reg;
            break;
        case TOK_NEWARRAY:
            reg = register_alloc("p");
            fprintf(oilfile, INDENT 
                    "%s* %s = xcalloc (%s, sizeof (%s));\n",
                    get_result_type_name(node->children[0]),
                    reg->c_str(),
                    node->children[1]->oilname->c_str(),
                    get_result_type_name(node->children[0]));
            node->oilname = reg;
            break;
        case TOK_NEWSTRING:
            reg = register_alloc("p");
            fprintf(oilfile, INDENT 
                    "char* %s = xcalloc (%s, sizeof (char));\n",
                    reg->c_str(), 
                    node->children[0]->oilname->c_str());
            node->oilname = reg;
            break;
        case TOK_NULL: case TOK_FALSE:
            node->oilname = new string("0");
            break;
        case TOK_TRUE:
            node->oilname = new string("1");
            break;
        case TOK_BLOCK: case TOK_ROOT:case TOK_STRINGCON:case ';':
            break;
        default:
            fprintf(stderr, "!!! unknown: %s\n", 
                    get_yytname(node->symbol));
            break;
    }
}

/* all functions are direct children of root. */
void emit_functions(astree *root)
{
    for(size_t child=0;child < root->children.size();child++) {
        astree *node = root->children[child];
        if(node->symbol == TOK_FUNCTION) {
            /* emit function return type and name */
            emit_recursive(node->children[0]);
            fprintf(oilfile, "%s(", 
                    node->children[0]->oilname->c_str());

            /* emit params */
            if(node->children[1]->children.size() == 0)
                fprintf(oilfile, "void");
            for(size_t param = 0;
                    param < node->children[1]->children.size();
                    param++) {

                if(!param) fprintf(oilfile, "\n");
                astree *parnode = node->children[1]->children[param];
                emit_recursive(parnode);
                fprintf(oilfile, INDENT);
                    fprintf(oilfile, "%s", parnode->oilname->c_str());
                if(param + 1 != node->children[1]->children.size())
                    fprintf(oilfile, ",\n");
            }
            fprintf(oilfile, ")\n");
            /* emit block */
            fprintf(oilfile, "{\n");
            emit_recursive(node->children[2]);
            fprintf(oilfile, "}\n");
        }
    }
}

/* globalstrings contains all string constants found during parse */
void emit_strings()
{
    for(size_t s=0;s<globalstrings.size();s++)
        fprintf(oilfile, "char* s%ld = %s;\n", s+1,
                globalstrings[s]->c_str());
}

/* all global variables are emitted at the top. */
void emit_globals(astree *root)
{
    for(size_t child = 0;child<root->children.size();child++) {
        astree *node = root->children[child];
        if(node->symbol == TOK_VARDECL) {
            /* recurse to generated oilnames */
            emit_recursive(node->children[0]);
            fprintf(oilfile, "%s;\n", 
                    node->children[0]->oilname->c_str());
        }
    }
}

/* emit all structures and their fields */
void emit_structs(astree *root)
{
    for(size_t child = 0;child<root->children.size();child++) {
        astree *node = root->children[child];
        if(node->symbol == TOK_STRUCT) {
            fprintf(oilfile, "struct s_%s {\n", 
                    node->children[0]->lexinfo->c_str());
            for(size_t field = 1;field < node->children.size();
                    field++) {
                astree *finode = node->children[field];
                /* recurse to generated oilnames */
                emit_recursive(finode);
                fprintf(oilfile, INDENT "%s;\n", 
                        finode->oilname->c_str());
            }
            fprintf(oilfile, "};\n");
        }
    }
}

int oc_run_emit(astree *root, FILE *out)
{
    oilfile = out;
    fprintf(oilfile, "#define __OCLIB_C__\n");
    fprintf(oilfile, "#include \"oclib.oh\"\n");
    emit_structs(root);
    emit_strings();
    emit_globals(root);
    emit_functions(root);

    fprintf(oilfile, "void __ocmain (void)\n{\n");
    emit_recursive(root);
    fprintf(oilfile, "}\n");
    return 0;
}

