#include <string>
#include <vector>
#include <cstdio>

#include "semantics.h"
#include "astree.h"
#include "lyutils.h"
using namespace std;
FILE *oilfile;

size_t reg_nr = 1, str_nr = 1;
#define INDENT "        "
vector<const string *> globalstrings;

static string *register_alloc(char *type)
{
    return new string(string(type) + to_string(reg_nr++));
}

void emitter_register_string(astree *node)
{
    node->oilname = new string(string("s") + to_string(str_nr++));
    globalstrings.push_back(node->lexinfo);
}

int test_is_operand(astree *node)
{
    return node->symbol == TOK_IDENT
        || node->symbol == TOK_INTCON
        || node->symbol == TOK_CHARCON;
}

void emit_recursive(astree *node)
{
    switch(node->symbol) {
        case TOK_STRUCT: case TOK_FUNCTION:
        case TOK_PROTOTYPE: case TOK_STRINGCON:
            return;
        case TOK_WHILE:
            fprintf(oilfile, "while_%ld_%ld_%ld:;\n",
                    node->filenr, node->linenr, node->offset);
            emit_recursive(node->children[0]);
            fprintf(oilfile, INDENT "if (!%s) goto break_%ld_%ld_%ld;\n",
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
    for(size_t child = 0;child < node->children.size();++child) {
        emit_recursive(node->children[child]);
    }
    //fprintf(stderr, ":: visiting: %d:%d:%d %s\n", node->filenr, node->linenr, node->offset, get_yytname(node->symbol));
    string *reg;
    const char *sym;
    switch(node->symbol) {
        case '+': case '-': case '*': case '>':
        case '/': case '%': case '<':
        case TOK_EQ: case TOK_NE: case TOK_LE:
        case TOK_GE: 
            node->oilname = register_alloc("i");
            fprintf(oilfile, INDENT "TYPE %s = %s %s %s;\n",
                    node->oilname->c_str(), 
                    node->children[0]->oilname->c_str(), 
                    node->lexinfo->c_str(),
                    node->children[1]->oilname->c_str());
            
            break;
        case TOK_POS: case TOK_NEG: case '!':
        case TOK_ORD: case TOK_CHR:
            node->oilname = register_alloc("i");
            if(node->symbol == TOK_ORD)
                sym = "(int)";
            else if(node->symbol == TOK_CHR)
                sym = "(char)";
            else
                sym = node->lexinfo->c_str();

            fprintf(oilfile, INDENT "TYPE %s = %s%s;\n",
                    node->oilname->c_str(), sym,
                    node->children[0]->oilname->c_str());
            break;
        case '=':
            node->oilname = node->children[0]->oilname;
            fprintf(oilfile, INDENT "%s = %s;\n",
                    node->children[0]->oilname->c_str(),
                    node->children[1]->oilname->c_str());
            break;
        case TOK_VARDECL: /* TODO: prevent globals */
            fprintf(oilfile, INDENT);
            if(node->parent->symbol != TOK_ROOT) {
                if(node->children[0]->symbol == TOK_ARRAY)
                    fprintf(oilfile, "%s* ", node->children[0]->children[0]->oilname->c_str());
                else
                    fprintf(oilfile, "%s ", node->children[0]->oilname->c_str());
            }
            if(node->children[0]->symbol == TOK_ARRAY)
                fprintf(oilfile, "%s ", node->children[0]->children[1]->oilname->c_str());
            else
                fprintf(oilfile, "%s ", node->children[0]->children[0]->oilname->c_str());
            fprintf(oilfile, "= %s;\n", node->children[1]->oilname->c_str());
            break;
        case TOK_CALL: /* TODO: void */
            node->oilname = register_alloc("i");
            fprintf(oilfile, INDENT "TYPE %s = %s(", node->oilname->c_str(),
                    node->children[0]->lexinfo->c_str());
            for(size_t child = 1;child < node->children.size();child++) {
                if(child != 1)
                    fprintf(oilfile, ", ");
                fprintf(oilfile, "%s", node->children[child]->oilname->c_str());
            }
            fprintf(oilfile, ");\n");
            break;
        case TOK_INTCON: case TOK_CHARCON:
            node->oilname = node->lexinfo;
            break;
        case TOK_RETURN:
            fprintf(oilfile, INDENT "return %s;\n", node->children[0]->oilname->c_str());
            break;
        case TOK_RETURNVOID:
            fprintf(oilfile, INDENT "return;\n");
            break;
        case TOK_ARRAY:

            break;
        case TOK_INDEX:
            reg = register_alloc("i");
            fprintf(oilfile, INDENT "TYPE %s = &%s[%s];\n",
                    reg->c_str(),
                    node->children[0]->oilname->c_str(),
                    node->children[1]->oilname->c_str());
            node->oilname = new string(string("*") + *reg);
            break;
        case '.':
            reg = register_alloc("i");
            fprintf(oilfile, INDENT "TYPE %s = &%s->%s;\n",
                    reg->c_str(), node->children[0]->oilname->c_str(),
                    node->children[1]->oilname->c_str());
            node->oilname = new string(string("*") + *reg);
            break;
        case TOK_IDENT:
            node->oilname = node->lexinfo; /* TODO: MANGLE */
            break;
        case TOK_DECLID:
            node->oilname = node->lexinfo; /* TODO: MANGLE */
            break;
        case TOK_FIELD:
            node->oilname = node->lexinfo; /* TODO: MANGLE */
            break;
        case TOK_INT: case TOK_CHAR: case TOK_BOOL: case TOK_VOID:
            node->oilname = node->lexinfo;
            break;
        case TOK_STRING:
            node->oilname = new string("char *");
            break;
        case TOK_TYPEID:
            node->oilname = new string(string("struct ") + *node->lexinfo + string("*"));
            break;
        case TOK_NEW: case TOK_NEWARRAY: case TOK_NEWSTRING:
            node->oilname = new string("XCALLOC");
            break;
        case TOK_NULL: case TOK_FALSE:
            node->oilname = new string("0");
            break;
        case TOK_TRUE:
            node->oilname = new string("1");
            break;
        case TOK_BLOCK: case TOK_ROOT:case TOK_STRINGCON: /* this is handled during parse */
            break;
        default:
            fprintf(stderr, "!!! unknown: %s\n", get_yytname(node->symbol));
            break;
    }
}

void emit_functions(astree *root)
{
    for(size_t child=0;child < root->children.size();child++) {
        astree *node = root->children[child];
        if(node->symbol == TOK_FUNCTION) {
            /* emit function return type and name */
            emit_recursive(node->children[0]);
            /* TODO: this code happens a lot... */
            if(node->children[0]->symbol == TOK_ARRAY)
                fprintf(oilfile, "%s* %s(", node->children[0]->children[0]->oilname->c_str(),
                        node->children[0]->children[1]->oilname->c_str());
            else
                fprintf(oilfile, "%s %s(", node->children[0]->oilname->c_str(),
                        node->children[0]->children[0]->oilname->c_str());
            /* emit params */
            if(node->children[1]->children.size() == 0)
                fprintf(oilfile, "void");
            for(size_t param = 0;param < node->children[1]->children.size();param++) {
                astree *parnode = node->children[1]->children[param];
                emit_recursive(parnode);
                fprintf(oilfile, INDENT);
                if(parnode->symbol == TOK_ARRAY)
                    fprintf(oilfile, "%s* %s", parnode->children[0]->oilname->c_str(),
                            parnode->children[1]->oilname->c_str());
                else
                    fprintf(oilfile, "%s %s", parnode->oilname->c_str(),
                            parnode->children[0]->oilname->c_str());
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

void emit_strings()
{
    for(size_t s=0;s<globalstrings.size();s++)
        fprintf(oilfile, "char *s%d = %s;\n", s+1, globalstrings[s]->c_str());
}

void emit_globals(astree *root)
{
    for(size_t child = 0;child<root->children.size();child++) {
        astree *node = root->children[child];
        if(node->symbol == TOK_VARDECL) {
            emit_recursive(node->children[0]);
            if(node->children[0]->symbol == TOK_ARRAY)
                fprintf(oilfile, "%s* ", node->children[0]->children[0]->oilname->c_str());
            else
                fprintf(oilfile, "%s ", node->children[0]->oilname->c_str());
            if(node->children[0]->symbol == TOK_ARRAY)
                fprintf(oilfile, "%s;\n", node->children[0]->children[1]->oilname->c_str());
            else
                fprintf(oilfile, "%s;\n", node->children[0]->children[0]->oilname->c_str());
        }
    }
}

void emit_structs(astree *root)
{
    for(size_t child = 0;child<root->children.size();child++) {
        astree *node = root->children[child];
        if(node->symbol == TOK_STRUCT) {
            fprintf(oilfile, "struct %s {\n", node->children[0]->lexinfo->c_str());
            for(size_t field = 1;field < node->children.size();field++) {
                astree *finode = node->children[field];
                emit_recursive(finode);
                if(finode->symbol == TOK_ARRAY)
                    fprintf(oilfile, INDENT "%s* ", finode->children[0]->oilname->c_str());
                else
                    fprintf(oilfile, INDENT "%s ", finode->oilname->c_str());
                if(finode->symbol == TOK_ARRAY)
                    fprintf(oilfile, "%s;\n", finode->children[1]->oilname->c_str());
                else
                    fprintf(oilfile, "%s;\n", finode->children[0]->oilname->c_str());
            }
            fprintf(oilfile, "};\n");
        }
    }
}

int oc_run_emit(astree *root, FILE *out)
{
#warning "TODO"
    oilfile = stderr;
    emit_structs(root);
    emit_strings();
    emit_globals(root);
    emit_functions(root);

    fprintf(oilfile, "void __ocmain (void)\n{\n");
    emit_recursive(root);
    fprintf(oilfile, "}\n");
    return 0;
}

