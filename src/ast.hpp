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

    virtual void Dump() const = 0;
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST
{
public:
    // 用智能指针管理对象
    std::unique_ptr<BaseAST> func_def;

    void Dump() const override
    {
        std::cout << "CompUnitAST { \n";
        func_def->Dump();
        std::cout << " }";
    }
};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_type; // 返回值类型
    std::string ident;                  // 函数名
    std::unique_ptr<BaseAST> block;     // 函数体

    void Dump() const override
    {
        std::cout << "FuncDefAST { \n";
        func_type->Dump();
        std::cout << ", " << ident << ", ";
        block->Dump();
        std::cout << " }";
    }
};

class FuncTypeAST : public BaseAST
{
public:
    std::string type;

    void Dump() const override
    {
        std::cout << "FuncTypeAST { \n"
                  << type << " }";
    }
};

class BlockAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> stmt; // 单行返回语句
    void Dump() const override
    {
        std::cout << "BlockAST { \n";
        stmt->Dump();
        std::cout << " }";
    }
};

class StmtAST : public BaseAST
{
public:
    std::string statement; // 单行返回语句
    void Dump() const override
    {
        std::cout << "BlockAST { \n"
                  << statement << " }\n";
    }
};
// ...

#endif