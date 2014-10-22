%{
// $Id: scanner.l,v 1.4 2014-10-20 14:27:17-07 - - $

#include "auxlib.h"
#include "lyutils.h"

#define YY_USER_ACTION  { scanner_useraction (); }
#define IGNORE(THING)   { }

%}

%option 8bit
%option debug
%option nodefault
%option nounput
%option noyywrap
%option verbose
%option warn

LETTER              [A-Za-z_]
DIGIT               [0-9]
CHARLIT_ITEM        ([^\\'\n]|\\[\\'"0nt])
CHARLIT_INV_ITEM    (\\[^\\'"0nt])
STRINGLIT_ITEM      ([^\\"\n]|\\[\\'"0nt])
STRINGLIT_INV_ITEM  (\\[^\\'"0nt])

NUMBER          ({DIGIT}+)
CHARACTER       ('{CHARLIT_ITEM}')
STRING          (\"{STRINGLIT_ITEM}*\")
IDENT           ({LETTER}({LETTER}|{DIGIT})*)

INV_STRING      ((\"{STRINGLIT_ITEM}*)(({STRINGLIT_INV_ITEM}+({STRINGLIT_ITEM}*(\"|\n)))|\n))
INV_IDENT       (({DIGIT}+){IDENT})
INV_CHARACTER   ('((({CHARLIT_ITEM}|{CHARLIT_INV_ITEM}){2,}('|\n))|({CHARLIT_INV_ITEM}*('|\n))|({CHARLIT_ITEM}?\n)))

%%

"#".*           { scanner_include(); }
[ \t]+          { IGNORE (white space) }
\n              { scanner_newline(); }

"void"          { return yylval_token(TOK_VOID); }
"bool"          { return yylval_token(TOK_BOOL); }
"char"          { return yylval_token(TOK_CHAR); }
"int"           { return yylval_token(TOK_INT); }
"string"        { return yylval_token(TOK_STRING); }
"struct"        { return yylval_token(TOK_STRUCT); }
"if"            { return yylval_token(TOK_IF); }
"else"          { return yylval_token(TOK_ELSE); }
"while"         { return yylval_token(TOK_WHILE); }
"return"        { return yylval_token(TOK_RETURN); }
"false"         { return yylval_token(TOK_FALSE); }
"true"          { return yylval_token(TOK_TRUE); }
"null"          { return yylval_token(TOK_NULL); }
"ord"           { return yylval_token(TOK_ORD); }
"chr"           { return yylval_token(TOK_CHR); }
"new"           { return yylval_token(TOK_NEW); }

{NUMBER}        { return yylval_token (TOK_INTCON); }
{CHARACTER}     { return yylval_token (TOK_CHARCON); }
{STRING}        { return yylval_token (TOK_STRINGCON); }
{IDENT}         { return yylval_token (TOK_IDENT); }

"="             { return yylval_token ('='); }
"+"             { return yylval_token ('+'); }
"-"             { return yylval_token ('-'); }
"*"             { return yylval_token ('*'); }
"/"             { return yylval_token ('/'); }
"^"             { return yylval_token ('^'); }
"("             { return yylval_token ('('); }
")"             { return yylval_token (')'); }
"["             { return yylval_token ('['); }
"]"             { return yylval_token (']'); }
"{"             { return yylval_token ('{'); }
"}"             { return yylval_token ('}'); }
";"             { return yylval_token (';'); }
","             { return yylval_token (','); }
"."             { return yylval_token ('.'); }
"<"             { return yylval_token ('<'); }
">"             { return yylval_token ('>'); }
"%"             { return yylval_token ('%'); }
"!"             { return yylval_token ('!'); }

"[]"            { return yylval_token (TOK_ARRAY); }
"=="            { return yylval_token (TOK_EQ); }
"!="            { return yylval_token (TOK_NE); }
"<="            { return yylval_token (TOK_LE); }
">="            { return yylval_token (TOK_GE); }

{INV_IDENT}     { scanner_invalidtoken(TOK_IDENT, yytext); } 
{INV_CHARACTER} { scanner_invalidtoken(TOK_CHARCON, yytext); } 
{INV_STRING}    { scanner_invalidtoken(TOK_STRINGCON, yytext); } 

.               { scanner_badchar (*yytext); }

%%

