# FeCompiler
My own compiler imitating MaxXing SysY Compiler(PKU)
src/koopa.h is only used for IntelliSense when developing the compiler.
When making the program, you should remove src/koopa.h, or the compiling process will fail.

## Grammar Definition (EBNF)
// pay attention to UB(Undefined Behavior), we can optimize a lot with ignoring UB. 

### added in Lv2:
CompUnit    ::= FuncDef;

FuncDef     ::= FuncType IDENT "(" ")" Block;
FuncType    ::= "int";

Block       ::= "{" Stmt "}";

### modified/added in Lv3.1 
// pay attention to UB(Undefined Behavior), we can optimize a lot with ignoring UB. 

Stmt        ::= "return" Exp ";";

Exp         ::= UnaryExp;
PrimaryExp  ::= "(" Exp ")" | Number;
Number      ::= INT_CONST;
UnaryExp    ::= PrimaryExp | UnaryOp UnaryExp;
UnaryOp     ::= "+" | "-" | "!";

### modified/added in Lv3.2

Exp         ::= UnaryExp | AddExp;

MulExp      ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
AddExp      ::= MulExp | AddExp ("+" | "-") MulExp;

### Complete Edition
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
