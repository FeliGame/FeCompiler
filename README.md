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

用EBNF（巴斯科范式）来表示文法定义，其在Flex和Yacc中实现，EBNF中[]表示其中的符号出现0次或1次；{}表示其中的符号出现0次或以上。

### EBNF modified now(Lv6.1)
```c++
Stmt ::= ...
       | ...
       | ...
       | "if" "(" Exp ")" Stmt ["else" Stmt] // 优先最近Else匹配
       | ...;
// 考虑到悬空else问题，改造为如下产生式（MS完全匹配，其后不能跟ELSE语句；UMS不完全匹配，右结合，其后可能可以跟else语句）
Stmt ::= MS | UMS

MS ::= ';' | Exp ';' | RETURN ';' | RETURN Exp ';' | LVal '=' Exp ';' | Block | IF '(' Exp ')' MS ELSE MS | WHILE '(' Exp ')' MS | BREAK ';' | CONTINUE ';'

UMS ::= IF '(' Exp ')' Stmt | IF '(' Exp ')' MS ELSE UMS | WHILE '(' Exp ')' UMS
```
#### Q&A
Q: 为什么if条件语句的EBNF中"if"和"("要分开？
A: 因为源代码中if和(可能中间有若干空格，如果将if和(分开，词法分析时会自动跳过空格进行分析。如果写成"if("，显然无法跳过空格。

Q: KoopaIR的基本块有什么特点，需要注意什么？
A: 基本块的结尾必须是 br, jump 或 ret 指令其中之一 (并且, 这些指令只能出现在基本块的结尾). 也就是说, 即使两个基本块是相邻的, 例如上述程序的 %else 基本块和 %end 基本块, 如果你想表达执行完前者之后执行后者的语义, 你也必须在前者基本块的结尾添加一条目标为后者的 jump 指令. 这点和汇编语言中 label 的概念有所不同.
但是有一个例外，那就是对于永远不可能到达的IR代码，autotest是不会通过的（坑了笔者很久！）例如
```c
%L0:
ret 0
jump %L1:     // KoopaIR报错！
%L1: ...
```
解决办法：设置综合属性`block_end`来表示当前代码块是不是终结。如果终结了就不能在下面继续生成代码了。能使得代码块终结的类型只有返回和跳转指令，在遇到这些指令时应设置`block_end`为true。

### EBNF in last edition(Lv5)
值得注意的是，如同C语言那样，Fe支持不同块的变/常量重名，但是Koopa IR不允许同一函数中定义相同的符号
```c++
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
Stmt ::= LVal "=" Exp ";"
       | [Exp] ";"
       | Block
       | "return" [Exp] ";";    

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
