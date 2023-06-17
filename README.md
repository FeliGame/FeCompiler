# FeCompiler
My own compiler imitating MaxXing SysY Compiler(PKU)
src/koopa.h is only used for IntelliSense when developing the compiler.
When making the program, you should remove src/koopa.h, or the compiling process will fail.

## Instruction
Fe语言相较于C语言，有如下特点：
* main函数中必须有返回语句
* 赋值表达式没有返回值
* 同一块中不能定义不同类型的同名变量
* 常量const声明的值必须在声明时初始化且不可作为赋值语句的左值（类似C const），且值必须在编译期确定（类似C++ constexp）

### EBNF in last edition(Lv3)
```C++
CompUnit    ::= FuncDef;

FuncDef     ::= FuncType IDENT "(" ")" Block;
FuncType    ::= "int";

Block       ::= "{" Stmt "}";
Stmt        ::= "return" Exp ";";

Exp         ::= LOrExp;
PrimaryExp  ::= "(" Exp ")" | Number;
Number      ::= INT_CONST;
UnaryExp    ::= PrimaryExp | UnaryOp UnaryExp;
UnaryOp     ::= "+" | "-" | "!";
MulExp      ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
AddExp      ::= MulExp | AddExp ("+" | "-") MulExp;
RelExp      ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
EqExp       ::= RelExp | EqExp ("==" | "!=") RelExp;
LAndExp     ::= EqExp | LAndExp "&&" EqExp;
LOrExp      ::= LAndExp | LOrExp "||" LAndExp;
```

### EBNF at now
```C++
CompUnit      ::= FuncDef;

Decl          ::= ConstDecl | VarDecl;
ConstDecl     ::= "const" BType ConstDef {"," ConstDef} ";";
BType         ::= "int";
ConstDef      ::= IDENT "=" ConstInitVal;
ConstInitVal  ::= ConstExp;
VarDecl       ::= BType VarDef {"," VarDef} ";";
VarDef        ::= IDENT | IDENT "=" InitVal;
InitVal       ::= Exp;

FuncDef       ::= FuncType IDENT "(" ")" Block;
FuncType      ::= "int";

Block         ::= "{" {BlockItem} "}";  // Many BlockItems are available!
BlockItem     ::= Decl | Stmt;
Stmt          ::= LVal "=" Exp ";"
                | "return" Exp ";";

Exp           ::= LOrExp;
LVal          ::= IDENT;
PrimaryExp    ::= "(" Exp ")" | LVal | Number;
Number        ::= INT_CONST;
UnaryExp      ::= PrimaryExp | UnaryOp UnaryExp;
UnaryOp       ::= "+" | "-" | "!";
MulExp        ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
AddExp        ::= MulExp | AddExp ("+" | "-") MulExp;
RelExp        ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
EqExp         ::= RelExp | EqExp ("==" | "!=") RelExp;
LAndExp       ::= EqExp | LAndExp "&&" EqExp;
LOrExp        ::= LAndExp | LOrExp "||" LAndExp;
ConstExp      ::= Exp;
```