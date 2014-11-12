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

%token TOK_ROOT TOK_BLOCK TOK_PARAMLIST TOK_DECLID
%token TOK_FUNCTION TOK_TYPEID TOK_FIELD TOK_VARDECL TOK_IFELSE
%token TOK_RETURNVOID TOK_NEWSTRING TOK_NEWARRAY TOK_INDEX TOK_CALL TOK_POS TOK_NEG

%start start

%right TOK_IF TOK_ELSE
%right '='
%left  TOK_EQ TOK_LE TOK_NE TOK_GE '>' '<'
%left  '+' '-'
%left  '*' '/' '%'
%right PREC_UPLUS PREC_UMINUS '!' TOK_ORG TOK_CHR
%left  PREC_INDEX PREC_FIELD PREC_CALL
%nonassoc TOK_NEW
%nonassoc PREC_PAREN

%%

start : program                     { yyparse_astree = $1; }
      ;

program : program statement         { $$ = adopt1($1, $2); }
        | program function          { $$ = adopt1($1, $2); }
        | program structdef         { $$ = adopt1($1, $2); }
        | program error '}'         { $$ = $1; }
        | program error ';'         { $$ = $1; }
        |                           { $$ = new_parseroot(); }
        ;

statement : expr ';'                { $$ = $1; }
          | block                   { $$ = $1; }
          | vardecl
          | whileloop
          | ifelse
          | returnstate
          ;

returnstate : TOK_RETURN expr ';'                              { $$ = adopt1($1, $2); }
            | TOK_RETURN ';'                                   { $1->symbol = TOK_RETURNVOID; $$ = $1;}
            ;

ifelse : TOK_IF '(' expr ')' statement                         { $$ = adopt2($1, $3, $5); }
       | TOK_IF '(' expr ')' statement TOK_ELSE statement      { $$ = adopt2($1, $3, $5); adopt1sym($1, $7, TOK_IFELSE); }
       ;

whileloop : TOK_WHILE '(' expr ')' statement                   { $$ = adopt2($1, $3, $5); }
          ;

vardecl : identdecl '=' expr ';'                               { $2->symbol = TOK_VARDECL; $$ = adopt2($2, $1, $3); }
        ;


block : blockcontents '}'                                      { $$ = $1; }
      | ';'                                                    { $$ = $1; }
      ;

blockcontents : blockcontents statement { $$ = adopt1($1, $2); }
              | '{'                     { $1->symbol = TOK_BLOCK; $$ = $1; }
              | '{' statement           { $$ = adopt1sym($1, $2, TOK_BLOCK); }
              ;

expr : expr '+' expr                    { $$ = adopt2($2, $1, $3); }
     | expr '-' expr                    { $$ = adopt2($2, $1, $3); }
     | expr '*' expr                    { $$ = adopt2($2, $1, $3); }
     | expr '/' expr                    { $$ = adopt2($2, $1, $3); }
     | expr '%' expr                    { $$ = adopt2($2, $1, $3); }
     | expr TOK_EQ expr                 { $$ = adopt2($2, $1, $3); }
     | expr TOK_NE expr                 { $$ = adopt2($2, $1, $3); }
     | expr TOK_LE expr                 { $$ = adopt2($2, $1, $3); }
     | expr TOK_GE expr                 { $$ = adopt2($2, $1, $3); }
     | expr '>' expr                    { $$ = adopt2($2, $1, $3); }
     | expr '<' expr                    { $$ = adopt2($2, $1, $3); }
     | expr '=' expr                    { $$ = adopt2($2, $1, $3); }
     | '-' expr %prec PREC_UMINUS       { $1->symbol = TOK_NEG; $$ = adopt1($1, $2); }
     | '+' expr %prec PREC_UPLUS        { $1->symbol = TOK_POS; $$ = adopt1($1, $2); }
     | '!' expr                         { $$ = adopt1($1, $2); }
     | TOK_ORD expr                     { $$ = adopt1($1, $2); }
     | TOK_CHR expr                     { $$ = adopt1($1, $2); }
     | '(' expr ')' %prec PREC_PAREN    { $$ = $2; }
     | constant                         { $$ = $1; }
     | variable                         { $$ = $1; }
     | allocator
     | call
     ;

call : TOK_IDENT '(' ')'                                { $2->symbol = TOK_CALL; $$ = adopt1($2, $1); }
     | callargs ')' %prec PREC_CALL                     { $$ = $1; }
     ;

callargs : callargs ',' expr                            { $$ = adopt1($1, $3); }
         | TOK_IDENT '(' expr                           { $2->symbol = TOK_CALL; $$ = adopt2($2, $1, $3); }
         ;

allocator : TOK_NEW TOK_IDENT '(' ')'                   { $2->symbol = TOK_TYPEID; $$ = adopt1($1, $2); }
          | TOK_NEW TOK_STRING '(' expr ')'             { $1->symbol = TOK_NEWSTRING; $$ = adopt1($1, $4); }
          | TOK_NEW basetype '[' expr ']'               { $1->symbol = TOK_NEWARRAY; $$ = adopt2($1, $2, $4); }
          ;

variable : TOK_IDENT
         | expr '[' expr ']' %prec PREC_INDEX       { $2->symbol = TOK_INDEX; $$ = adopt2($2, $1, $3); }
         | expr '.' TOK_IDENT %prec PREC_FIELD      { $3->symbol = TOK_FIELD; $$ = adopt2($2, $1, $3); }
         ;

constant : TOK_INTCON               { $$ = $1; }
         | TOK_STRINGCON            { $$ = $1; }
         | TOK_CHARCON              { $$ = $1; }
         | TOK_TRUE                 { $$ = $1; }
         | TOK_FALSE                { $$ = $1; }
         | TOK_NULL                 { $$ = $1; }
         ;

function : identdecl funcargs ')' block   { $$ = tree_function($1, $2, $4); }
         | identdecl '(' ')' block        { $2->symbol = TOK_PARAMLIST; $$ = tree_function($1, $2, $4); }
         ;

funcargs : funcargs ',' identdecl         { $$ = adopt1($1, $3); }
         | '(' identdecl                  { $1->symbol = TOK_PARAMLIST; $$ = $2 ? adopt1($1, $2) : $1; }
         ;

identdecl : basetype TOK_IDENT            { $$ = adopt1sym($1, $2, TOK_DECLID); }
          | basetype TOK_ARRAY TOK_IDENT  { $$ = adopt2($2, $1, $3); }
          ;

basetype : TOK_VOID
         | TOK_BOOL
         | TOK_INT
         | TOK_CHAR
         | TOK_STRING
         | TOK_IDENT                      { $1->symbol = TOK_TYPEID; $$ = $1; }
         ;

structdef : structcont '}'                { $$ = $1; }
          ;

structcont : structcont fielddecl ';'     { $$ = adopt1($1, $2); }
           | TOK_STRUCT TOK_IDENT '{'     { $2->symbol = TOK_TYPEID; $$ = adopt1($1, $2); }
           ;

fielddecl : basetype TOK_IDENT            { $$ = adopt1sym($1, $2, TOK_FIELD); }
          | basetype TOK_ARRAY TOK_IDENT  { $$ = adopt2($2, $1, $3); }
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

