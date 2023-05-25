#ifndef AST_HPP
#define AST_HPP

#include <memory>
#include <string>
#include <iostream>
#include <cassert>

using namespace std;

// 所有 AST 的基类
class BaseAST
{
public:
    int64_t t_id = -1; // 表达式计算结果的变量名序号
    string t_val;      // 表达式计算结果的值
    enum ValueType {VT_INT} t_type;   // 表达式计算结果的类型

    virtual ~BaseAST() = default;
    // Dump translates the AST into Koopa IR string
    virtual string Dump() = 0;

    // 获取引用名（常作为左值）
    string Get_ref()
    {
        return "%" + to_string(t_id);
    }

    // 如有可能，获取引用名（常作为右值）
    string Get_ref_if_possible()
    {
        return (t_id == -1 ? t_val : Get_ref());
    }
};

class CompUnitAST : public BaseAST
{
public:
    // 用智能指针管理对象
    unique_ptr<BaseAST> func_def;

    string Dump()   override
    {
        string s = func_def->Dump();
        return s;
    }
};

class FuncDefAST : public BaseAST
{
public:
    unique_ptr<BaseAST> func_type; // 返回值类型
    string ident;                  // 函数名
    unique_ptr<BaseAST> block;     // 函数体

    string Dump()   override
    {
        string s = "fun @" + ident + "(): " + func_type->Dump() + " {\n%entry:\n" + block->Dump() + "}";
        return s;
    }
};

class FuncTypeAST : public BaseAST
{
public:
    string type;

    string Dump()   override
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
    unique_ptr<BaseAST> stmt; // 一个分号对应一个stmt

    string Dump()   override
    {
        string s = stmt->Dump();
        return s;
    }
};

class StmtAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp; // 表达式

    string Dump()   override
    {
        // 对于return +(- -!6); 应当输出：
        // %0 = eq 6, 0
        // %1 = sub 0, %0
        // %2 = sub 0, %1
        // ret %2
        string s = exp->Dump() + "ret " + exp->Get_ref_if_possible() + "\n"; // 指令行
        return s;
    }
};

// 狭义表达式（一元、二元）
class ExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> unaryExp; // 目前仅支持一元表达式

    string Dump()   override
    {
        // 值不发生改变，因此原样复制
        string s = unaryExp->Dump();
        t_id = unaryExp->t_id;
        t_val = unaryExp->t_val;
        t_type = unaryExp->t_type;
        return s;
    }
};

// 主表达式（一元、二元、整数）
class PrimaryExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> exp; // 表达式
    // 产生式2
    uint32_t number; // 整数

    string Dump()   override
    {
        string s;
        switch (selection)
        {
        case 1:
            // 6. (- -!6) ，没有计算指令
            // 值不发生改变，因此原样复制
            t_id = exp->t_id;
            t_val = exp->t_val;
            t_type = exp->t_type;
            s = exp->Dump();
            break;
        case 2:
            // 1.该式子规约6为Number，没有计算指令，且单个变量/常量无需输出值
            t_id = -1;
            t_val = to_string(number);
            t_type = VT_INT;
            break;
        default:
            cerr << "Parsing Error in: PrimaryExp:=(Exp)|Number\n";
            assert(false);
        }
        return s;
    }
};

// 一元表达式
class UnaryExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> primaryExp; // 表达式
    // 产生式2
    string unaryOp;               // 一元算符
    unique_ptr<BaseAST> unaryExp; // 一元表达式

    string Dump()   override
    {
        string s;
        switch (selection)
        {
        case 1:
            s = primaryExp->Dump();
            // 2.该式子规约Number(6)，没有计算指令
            t_id = primaryExp->t_id;
            t_val = primaryExp->t_val;
            t_type = primaryExp->t_type;
            break;
        case 2:
            // 3. 第一次计算：!6 => %0 = eq 6, 0 他知道是eq 6 0，是因为被计算值非零
            // 4. 第二次计算：-!6 => %1 = sub 0, %0
            // 5. 第三次计算：- -!6 => %2 = sub 0, %1
            // 7. +(- -!6) "+"计算不会产生新的IR，没有计算和其他操作
            t_type = unaryExp->t_type;
            if (unaryOp == "!")
            {
                s = unaryExp->Dump();   // 先计算子表达式的值
                uint32_t unaryExp_val = atoi(unaryExp->t_val.data()); // 获取子表达式的值，并转换为整型
                t_id = unaryExp->t_id + 1;                            // 分配新临时变量
                s = s + Get_ref() + " = eq " + unaryExp->Get_ref_if_possible() + ", " + to_string(!unaryExp_val) + "\n";
            }
            else if (unaryOp == "-")
            {
                s = unaryExp->Dump();
                t_id = unaryExp->t_id + 1; // 分配新临时变量
                s = s + Get_ref() + " = sub 0, " + unaryExp->Get_ref_if_possible() + "\n";
            }
            else if (unaryOp == "+")    // 不产生新指令，照搬之前的指令
            {
                s = unaryExp->Dump();
            }
            break;
        default:
            cerr << "Parsing Error in: UnaryExp:=PrimaryExp|UnaryOp UnaryExp\n";
            assert(false);
        }
        return s;
    }
};
// ...

#endif // AST_HPP