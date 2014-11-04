%{
// Dummy parser for scanner project.

#include "lyutils.h"
#include "astree.h"
#include <cassert>
%}

%debug
%defines
%error-verbose
%token-table
%verbose

//%token TOK_ARRAY TOK_INTCON TOK_CHARCON TOK_STRINGCON
//%token TOK_BLOCK TOK_CALL TOK_IFELSE TOK_INITDECL TOK_POS TOK_NEG
//%token TOK_NEWARRAY TOK_TYPEID TOK_FIELD

// reserved words
%token TOK_VOID TOK_BOOL TOK_CHAR TOK_INT TOK_STRING
%token TOK_IF TOK_ELSE TOK_WHILE TOK_RETURN TOK_STRUCT
%token TOK_FALSE TOK_TRUE TOK_NULL TOK_NEW TOK_ORD TOK_CHR

// two-character symbols
%token TOK_EQ TOK_NE TOK_LE TOK_GE TOK_ARRAY

// patterns
%token TOK_IDENT TOK_INTCON TOK_CHARCON TOK_STRINGCON

%token TOK_ROOT

%start program

%%

program : program token | ;
token   : '(' | ')' | '[' | ']' | '{' | '}' | ';' | ',' | '.'
        | '=' | '+' | '-' | '*' | '/' | '%' | '!' | '<' | '>'
        | TOK_VOID | TOK_BOOL | TOK_CHAR | TOK_INT | TOK_STRING
        | TOK_IF | TOK_ELSE | TOK_WHILE | TOK_RETURN | TOK_STRUCT
        | TOK_FALSE | TOK_TRUE | TOK_NULL | TOK_NEW | TOK_ARRAY
        | TOK_EQ | TOK_NE | TOK_LE | TOK_GE
        | TOK_IDENT | TOK_INTCON | TOK_CHARCON | TOK_STRINGCON
        | TOK_ORD | TOK_CHR | TOK_ROOT
        ;

%%

const char *get_yytname (int symbol) {
   return yytname [YYTRANSLATE (symbol)];
}


bool is_defined_token (int symbol) {
   return YYTRANSLATE (symbol) > YYUNDEFTOK;
}
/*
static void* yycalloc (size_t size) {
   void* result = calloc (1, size);
   assert (result != NULL);
   return result;
}
*/
