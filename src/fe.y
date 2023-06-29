/*
%code [qualifier] { code_seg }
qualifier有如下几种值：
空：和%{%}没有区别
requires：code_seg只能是C/C++，将被放在声明文件（也就是--defines生成的文件）和实现文件（也就是--output生成的文件）中定义YYSTYPE、YYLTYPE之前。可以将包含的头文件放在这里。
provides：code_seg只能是C/C++，将被放在声明文件和实现文件中定义YYSTYPE、YYLTYPE之后。这里可以放其他文件会用到的一些变量或函数的声明。比如yylex的声明。
top：code_seg只能是C/C++，将被放在实现文件的最开头。
imports：此时代码段code_seg只能是JAVA
*/
%code requires {
  #include <memory>
  #include <string>
  #include "ast.hpp"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include "ast.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

// 声明 lexer 函数和错误处理函数
// C++11 unique_ptr: one ptr only reflects on one r-val; it destructs automatically.
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// 定义 yyparse() 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
// %parse-param { std::unique_ptr<std::string> &ast }

// Lv1.3
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval(token流) 的定义, 我们把它定义成了一个联合体 (union)
// all union members share same mem and address
// 因为 token 的值有的是字符串指针, 有的是整数(single choice)
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
// union must got destructor redefined!
// however union prohibit abstract constructor!
// %union {
//   std::string *str_val;
//   int int_val;
// }

// str_val or int_val is the leaf node of ast
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
}

// %token: <decl in %union, values>
// lexer 返回的所有 终结符（即token） 类型的声明
%token INT RETURN CONST IF ELSE
%token <str_val> IDENT RELOP EQOP LOGICAND LOGICOR
%token <int_val> INT_VAL

%type <ast_val> Decl ConstDecl BType ConstDefs ConstDef ConstInitVal VarDecl VarDefs VarDef InitVal BlockItems BlockItem LVal ConstExp
  FuncDef FuncType Block Stmt MS UMS Exp PrimaryExp UnaryExp AddExp MulExp RelExp EqExp LAndExp LOrExp
%type <int_val> Number
%type <str_val> UnaryOp

%%

CompUnit
  : FuncDef {
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_def = unique_ptr<BaseAST>($1);
    ast = move(comp_unit);  // move()函数强制将左值转换为右值
  }
  ;

Decl
  : ConstDecl {
    auto ast = new DeclAST();
    ast->selection = 1;
    ast->const_decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | VarDecl {
    auto ast = new DeclAST();
    ast->selection = 2;
    ast->var_decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

ConstDecl
  : CONST BType ConstDefs ';' {
    auto ast = new ConstDeclAST();
    ast->b_type = unique_ptr<BaseAST>($2);
    ast->const_defs = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

ConstDefs
  : ConstDefs ',' ConstDef {
    auto ast = new ConstDefsAST();
    ast->selection = 1;
    ast->const_defs = unique_ptr<BaseAST>($1);
    ast->const_def = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | ConstDef {
    auto ast = new ConstDefsAST();
    ast->selection = 2;
    ast->const_def = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

BType
  : INT {
    auto ast = new BTypeAST();
    ast->type = "int";
    $$ = ast;
  }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->const_init_val = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

ConstInitVal
  : ConstExp {
    auto ast = new ConstInitValAST();
    ast->const_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

VarDecl 
  : BType VarDefs ';' {
    auto ast = new VarDeclAST();
    ast->b_type = unique_ptr<BaseAST>($1);
    ast->var_defs = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

VarDefs
  : VarDefs ',' VarDef {
    auto ast = new VarDefsAST();
    ast->selection = 1;
    ast->var_defs = unique_ptr<BaseAST>($1);
    ast->var_def = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | VarDef {
    auto ast = new VarDefsAST();
    ast->selection = 2;
    ast->var_def = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

VarDef
  : IDENT {
    auto ast = new VarDefAST();
    ast->selection = 1;
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '=' InitVal {
    auto ast = new VarDefAST();
    ast->selection = 2;
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

InitVal
  : Exp {
    auto ast = new InitValAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

FuncDef
  : FuncType IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;

FuncType
  : INT {
    auto ast = new FuncTypeAST();
    ast->type = "int";
    $$ = ast;
  }
  ;

Block
  : '{' '}' {
    // do nothing
    auto ast = new BlockAST();
    ast->isNull = true;
    $$ = ast;
  }
  | '{' BlockItems '}' {
    auto ast = new BlockAST();
    ast->isNull = false;
    ast->block_items = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

// 左递归，方便yacc的LR分析
BlockItems
  : BlockItems BlockItem {
    auto ast = new BlockItemsAST();
    ast->selection = 1;
    ast->block_items = unique_ptr<BaseAST>($1);
    ast->block_item = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | BlockItem {
    auto ast = new BlockItemsAST();
    ast->selection = 2;
    ast->block_item = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

BlockItem
  : Decl {
    auto ast = new BlockItemAST();
    ast->selection = 1;
    ast->decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | Stmt {
    auto ast = new BlockItemAST();
    ast->selection = 2;
    ast->stmt = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

Stmt
  : MS {
    auto ast = new StmtAST();
    ast->ms_ums = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | UMS {
    auto ast = new StmtAST();
    ast->ms_ums = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

MS  
  : LVal '=' Exp ';' {
    auto ast = new MSAST();
    ast->selection = 1;
    ast->l_val = unique_ptr<BaseAST>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new MSAST();
    ast->selection = 2;
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | ';' {
    auto ast = new MSAST();
    ast->selection = 0;
    $$ = ast;
  }
  | Block {
    auto ast = new MSAST();
    ast->selection = 3;
    ast->block = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | IF '(' Exp ')' MS ELSE MS {
    auto ast = new MSAST();
    ast->selection = 4;
    ast->exp = unique_ptr<BaseAST>($3);
    ast->ms = unique_ptr<BaseAST>($5);
    ast->else_ms = unique_ptr<BaseAST>($7);
    $$ = ast;
  }
  | RETURN ';' {
    auto ast = new MSAST();
    ast->selection = 0;
    $$ = ast;
  }
  | RETURN Exp ';' {
    auto ast = new MSAST();
    ast->selection = 5;
    ast->exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  } 
  ;

  UMS  
  : IF '(' Exp ')' MS ELSE UMS {
    auto ast = new UMSAST();
    ast->selection = 1;
    ast->exp = unique_ptr<BaseAST>($3);
    ast->ms = unique_ptr<BaseAST>($5);
    ast->ums = unique_ptr<BaseAST>($7);
    $$ = ast;
  }
  | IF '(' Exp ')' Stmt {
    auto ast = new UMSAST();
    ast->selection = 2;
    ast->exp = unique_ptr<BaseAST>($3);
    ast->stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;

Exp         
  : 
  // UnaryExp {
  //   auto ast = new ExpAST();
  //   ast->selection = 1;
  //   ast->unaryExp = unique_ptr<BaseAST>($1);
  //   $$ = ast;
  // }
  // | AddExp {
  //   auto ast = new ExpAST();
  //   ast->selection = 2;
  //   ast->addExp = unique_ptr<BaseAST>($1);
  //   $$ = ast;
  // }
  // | 
  LOrExp {
    auto ast = new ExpAST();
    ast->selection = 3;
    ast->lorExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
;

LVal
  : IDENT {
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  ;

PrimaryExp  
  : '(' Exp ')' {
    auto ast = new PrimaryExpAST();
    ast->selection = 1;
    ast->exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | LVal {
    auto ast = new PrimaryExpAST();
    ast->selection = 2;
    ast->l_val = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | Number {
    auto ast = new PrimaryExpAST();
    ast->selection = 3;
    ast->number = $1; // it's of int32
    $$ = ast;
  }
;

Number
  : INT_VAL {
    $$ = $1;
  }
;

UnaryExp    
  : PrimaryExp {
    auto ast = new UnaryExpAST();
    ast->selection = 1;
    ast->primaryExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  } 
  | UnaryOp UnaryExp {
    auto ast = new UnaryExpAST();
    ast->selection = 2;
    ast->unaryOp = *unique_ptr<string>($1);
    ast->unaryExp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
;

UnaryOp
  : '+' { $$ = new string("+"); }
  | '-' { $$ = new string("-"); }
  | '!' { $$ = new string("!"); }
;

// 多元表达式
MulExp
  : UnaryExp {
    auto ast = new MulExpAST();
    ast->selection = 1;
    ast->unaryExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | MulExp '*' UnaryExp {
    auto ast = new MulExpAST();
    ast->selection = 2;
    ast->mulExp = unique_ptr<BaseAST>($1);
    ast->mulop = "*";
    ast->unaryExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | MulExp '/' UnaryExp {
    auto ast = new MulExpAST();
    ast->selection = 2;
    ast->mulExp = unique_ptr<BaseAST>($1);
    ast->mulop = "/";
    ast->unaryExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | MulExp '%' UnaryExp {
    auto ast = new MulExpAST();
    ast->selection = 2;
    ast->mulExp = unique_ptr<BaseAST>($1);
    ast->mulop = "%";
    ast->unaryExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
;

AddExp
  : MulExp {
    auto ast = new AddExpAST();
    ast->selection = 1;
    ast->mulExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp '+' MulExp {
    auto ast = new AddExpAST();
    ast->selection = 2;
    ast->addExp = unique_ptr<BaseAST>($1);
    ast->mulop = "+";
    ast->mulExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | AddExp '-' MulExp {
    auto ast = new AddExpAST();
    ast->selection = 2;
    ast->addExp = unique_ptr<BaseAST>($1);
    ast->mulop = "-";
    ast->mulExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
;

RelExp
  : AddExp {
    auto ast = new RelExpAST();
    ast->selection = 1;
    ast->addExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | RelExp RELOP AddExp {
    auto ast = new RelExpAST();
    ast->selection = 2;
    ast->relExp = unique_ptr<BaseAST>($1);
    ast->relop = *$2;
    ast->addExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
;

EqExp
  : RelExp {
    auto ast = new EqExpAST();
    ast->selection = 1;
    ast->relExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | EqExp EQOP RelExp {
    auto ast = new EqExpAST();
    ast->selection = 2;
    ast->eqExp = unique_ptr<BaseAST>($1);
    ast->eqop = *$2;
    ast->relExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
;

LAndExp
  : EqExp {
    auto ast = new LAndExpAST();
    ast->selection = 1;
    ast->eqExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LAndExp LOGICAND EqExp {
    auto ast = new LAndExpAST();
    ast->selection = 2;
    ast->landExp = unique_ptr<BaseAST>($1);
    ast->eqExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
;

LOrExp
  : LAndExp {
    auto ast = new LOrExpAST();
    ast->selection = 1;
    ast->landExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp LOGICOR LAndExp {
    auto ast = new LOrExpAST();
    ast->selection = 2;
    ast->lorExp = unique_ptr<BaseAST>($1);
    ast->landExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
;

ConstExp
  : Exp {
    auto ast = new ConstExpAST();
    ast->exp = unique_ptr<BaseAST>($1);
  }
%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  extern int yylineno;	// defined and maintained in lex
	extern char *yytext;	// defined and maintained in lex
	int len=strlen(yytext);
	int i;
	char buf[512]={0};
	for (i=0;i<len;++i)
	{
		sprintf(buf,"%s%d ",buf,yytext[i]);
	}
	fprintf(stderr, "PARSER ERROR: %s at symbol '%c' on line %d\n", s, atoi(buf), yylineno);
}
