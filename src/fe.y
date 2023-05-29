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

// def of ast, Lv1.3
// str_val or int_val is the leaf node of ast
%union {
  std::string *str_val;
  // std::string *op_val;
  int int_val;
  BaseAST *ast_val;
}

// %token: <decl in %union, values>
// lexer 返回的所有 终结符（即token） 类型的声明
// 注意 IDENT 和 INT_VAL 会返回 token 的值, 分别对应 str_val 和 int_val
// str_val is a pointer type, we say that it's of IDENT
%token INT RETURN
%token <str_val> IDENT RELOP LOGICAND LOGICOR
%token <int_val> INT_VAL
// %token <op_val> OP

// 非终结符的类型定义，ast_val的数据类型会被映射到右边的所有非终结符
// str_val include these NonTerminal Symbols
// %type can be seen as content type of str_val
// Lv3.1
%type <ast_val> FuncDef FuncType Block Stmt Exp PrimaryExp UnaryExp AddExp MulExp RelExp EqExp LAndExp LOrExp
%type <int_val> Number
%type <str_val> UnaryOp

%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情（SDT）
// 之前我们定义了 FuncDef 会返回一个 str_val, 也就是字符串指针
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值
CompUnit
  : FuncDef {
    // Lv1.3，comp_unit is the ast root, no member creates it
    // so you should make_unique to init comp_unit.
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_def = unique_ptr<BaseAST>($1);
    ast = move(comp_unit);  // move()函数强制将左值转换为右值
  }
  ;

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是字符串指针了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的字符串指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
FuncDef
  : FuncType IDENT '(' ')' Block {
    // Lv 1.2(just output src identically)
    // auto type = unique_ptr<string>($1);
    // auto ident = unique_ptr<string>($2);
    // auto block = unique_ptr<string>($5);
    // $$ = new string(*type + " " + *ident + "() " + *block);
    
    // Lv1.3
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;

// Terminal Symbols, they are leaf nodes
FuncType
  : INT {
    auto ast = new FuncTypeAST();
    ast->type = "int";
    $$ = ast;
  }
  ;

Block
  : '{' Stmt '}' {
    auto ast = new BlockAST();
    ast->stmt = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

Stmt
  // : RETURN Number ';' {
  : RETURN Exp ';' {
    auto ast = new StmtAST();
    // ast->statement = to_string($2);
    ast->exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

Exp         
  : UnaryExp {
    auto ast = new ExpAST();
    ast->selection = 1;
    ast->unaryExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp {
    auto ast = new ExpAST();
    ast->selection = 2;
    ast->addExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp {
    auto ast = new ExpAST();
    ast->selection = 3;
    ast->lorExp = unique_ptr<BaseAST>($1);
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
  | Number {
    auto ast = new PrimaryExpAST();
    ast->selection = 2;
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
  | EqExp RELOP RelExp {
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
