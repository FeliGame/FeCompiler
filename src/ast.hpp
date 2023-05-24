#ifndef AST_HPP
#define AST_HPP

#include <memory>
#include <string>
#include <iostream>

using namespace std;

// 所有 AST 的基类
class BaseAST
{
public:
    virtual ~BaseAST() = default;
    // Dump translates the AST into Koopa IR string
    virtual string Dump() const = 0;
};

class CompUnitAST : public BaseAST
{
public:
    // 用智能指针管理对象
    unique_ptr<BaseAST> func_def;

    string Dump() const override
    {
        // cout << "CompUnitAST { \n";
        string s = func_def->Dump();
        // cout << " }";
        return s;
    }
};

class FuncDefAST : public BaseAST
{
public:
    unique_ptr<BaseAST> func_type; // 返回值类型
    string ident;                  // 函数名
    unique_ptr<BaseAST> block;     // 函数体

    string Dump() const override
    {
        string s = "fun @" + ident + "(): " + func_type->Dump() + " {\n%entry:\n\t" + block->Dump() + "\n}";
        return s;
    }
};

class FuncTypeAST : public BaseAST
{
public:
    string type;

    string Dump() const override
    {
        string s = "\0";
        if (type == "int")
            s = "i32";
        return s;
    }
};

class BlockAST : public BaseAST
{
public:
    unique_ptr<BaseAST> stmt; // 单行返回语句
    string Dump() const override
    {
        // cout << "BlockAST { \n";
        string s = stmt->Dump();
        // cout << " }";
        return s;
    }
};

class StmtAST : public BaseAST
{
public:
    string statement; // 单行返回语句
    string Dump() const override
    {
        string s = "ret " + statement;
        return s;
    }
};
// ...

#endif  // AST_HPP