%option noyywrap
%option nounput
%option noinput

%{
/* OPTIONS: 
noyywrap    扫描单文件，不调用yywrap()
nounput     unput将字符插入输入流
noinput     input返回输入流的下一单个字符
这些选项通常都开启，除非特殊使用
*/

// 该部分写生成的C/C++所需的全局代码(宏定义、全局声明)

#include <cstdlib>
#include <string>
#include <iostream>

// 因为 Flex 会用到 Bison 中关于 token 的定义
// 所以需要 include Bison 生成的头文件
#include "fe.tab.hpp"

using namespace std;

%}
/* Flex所需的某个符号的正则表达式 */
/* Bison所需的终结符和非终结符的类型*/

/* 空白符和注释 */
WhiteSpace    [ \t\n\r]*
LineComment   "//".*
/* BlockComment added, it should be shortest match*/
BlockComment    "/*"[^(*/)]*"*/"

/* 标识符（字母或下划线开头） */
Identifier    [a-zA-Z_][a-zA-Z0-9_]*

/* 整数字面量 */
Decimal       [1-9][0-9]*
Octal         0[0-7]*
Hexadecimal   0[xX][0-9a-fA-F]+

RelativeOperator    (<=)|(>=)|(<)|(>)
EqualOperator       (!=)|(==)
/* Add: Even -s can be ignored */
/* RedundantNeg  ("--")* */


/* %%包围的部分是Flex/Bison的规则描述（词法分析、语法分析） */
/* 下面的规则，字符串越长的越在前面（优先匹配）*/
%%

{WhiteSpace}    { /* 忽略, 不做任何操作 */ }
{LineComment}   { /* 忽略, 不做任何操作 */ }
{BlockComment}  { /* nothing to do */ }

"int"           { return INT; }
"return"        { return RETURN; }

{RelativeOperator} { yylval.str_val = new string(yytext); return RELOP; }
{EqualOperator} {yylval.str_val = new string(yytext); return EQOP;}
"&&"            { yylval.str_val = new string(yytext); return LOGICAND; }
"||"            { yylval.str_val = new string(yytext); return LOGICOR; }

{Identifier}    { 
    // lexer transfers yylval(tokens) to parser
    // str_val and int_val are defined in fe.y
    yylval.str_val = new string(yytext);
    return IDENT;
    }

{Decimal}       { yylval.int_val = strtol(yytext, nullptr, 0); return INT_VAL; }
{Octal}         { yylval.int_val = strtol(yytext, nullptr, 0); return INT_VAL; }
{Hexadecimal}   { yylval.int_val = strtol(yytext, nullptr, 0); return INT_VAL; }

.               { return yytext[0]; } // it matches any single token except \n.

%%

/* 用户自定义代码，例如在生成C/C++中的辅助函数定义，可以给上面的规则调用，声明需要写在文件开头 */
/* if lexer error, output that. */
void lexerror(const char *s) {
    cerr << "lex error: " << s << endl;
}