#ifndef AST_HPP
#define AST_HPP

#include <memory>
#include <string>
#include <iostream>
#include <cassert>

using namespace std;
// 不能全局声明变量和函数！！！

// 全局临时变量使用记录
static bool global_t_id[1024];

// 所有 AST 的基类
class BaseAST
{
public:
    int32_t t_id = -1; // 表达式计算结果的变量名序号
    // 【可优化】如果一个式子规约到出现第一条IR时，此后无需再计算t_val值，除非想优化
    string t_val; // 表达式计算结果的值，主要是因为部分指令对值没有影响，因此需要通过它来继承
    enum ValueType
    {
        VT_INT
    } t_type; // 表达式计算结果的类型

    virtual ~BaseAST() = default;
    // Dump translates the AST into Koopa IR string
    virtual string Dump() = 0;

    // 从全局t_id分配新的t_id给当前节点
    void alloc_ref()
    {
        for(size_t i = 0; i < 1024; ++i) {
            if(!global_t_id[i]) {
                t_id = i;
                global_t_id[i] = true;
                return;
            }
        }
    }
    // 释放（尽可能用少的t_id，减少寄存器浪费空间）
    void free_ref(int32_t t_id)
    {
        if(t_id == -1) return;
        global_t_id[t_id] = false;
    }

    // 获取引用名（常作为左值）
    string get_ref()
    {
        return "%" + to_string(t_id);
    }

    // 如有可能，获取引用名（常作为右值）
    string get_ref_if_possible()
    {
        return (t_id == -1 ? t_val : get_ref());
    }

    string get_koopa_op(string op)
    {
        if (op == "*")
            return "mul";
        else if (op == "/")
            return "div";
        else if (op == "%")
            return "mod";
        else if (op == "+")
            return "add";
        else if (op == "-")
            return "sub";
        else if (op == ">=")
            return "ge";
        else if (op == "<=")
            return "le";
        else if (op == ">")
            return "gt";
        else if (op == "<")
            return "lt";
        else if (op == "==")
            return "eq";
        else if (op == "!=")
            return "ne";
        assert(false);
    }
};

class CompUnitAST : public BaseAST
{
public:
    // 用智能指针管理对象
    unique_ptr<BaseAST> func_def;

    string Dump() override
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

    string Dump() override
    {
        string s = "fun @" + ident + "(): " + func_type->Dump() + " {\n%entry:\n" + block->Dump() + "}";
        return s;
    }
};

class FuncTypeAST : public BaseAST
{
public:
    string type;

    string Dump() override
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

    string Dump() override
    {
        string s = stmt->Dump();
        return s;
    }
};

class StmtAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp; // 表达式

    string Dump() override
    {
        string s = exp->Dump();
        s = s + "ret " + exp->get_ref_if_possible() + "\n"; // 指令行
        return s;
    }
};

// 狭义表达式（一元、二元）
class ExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    uint32_t selection;
    unique_ptr<BaseAST> unaryExp; // 一元表达式
    unique_ptr<BaseAST> addExp;   // 加法表达式
    unique_ptr<BaseAST> lorExp;

    string Dump() override
    {
        string s;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            s = unaryExp->Dump();
            t_id = unaryExp->t_id;
            t_val = unaryExp->t_val;
            t_type = unaryExp->t_type;
            break;
        case 2:
            // 值不发生改变，因此原样复制
            s = addExp->Dump();
            t_id = addExp->t_id;
            t_val = addExp->t_val;
            t_type = addExp->t_type;
            break;
        case 3:
            // 值不发生改变，因此原样复制
            s = lorExp->Dump();
            t_id = lorExp->t_id;
            t_val = lorExp->t_val;
            t_type = lorExp->t_type;
            break;
        default:
            assert(false);
        }
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

    string Dump() override
    {
        string s;
        switch (selection)
        {
        case 1:
            // 6. (- -!6) ，没有计算指令
            // 值不发生改变，因此原样复制
            s = exp->Dump();
            t_id = exp->t_id;
            t_val = exp->t_val;
            t_type = exp->t_type;
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

    string Dump() override
    {
        string s;
        switch (selection)
        {
        case 1:
            s = primaryExp->Dump();
            t_id = primaryExp->t_id;
            t_val = primaryExp->t_val;
            t_type = primaryExp->t_type;
            break;
        case 2:
            t_type = unaryExp->t_type;
            if (unaryOp == "!")
            {
                s = unaryExp->Dump();      // 先计算子表达式的值
                alloc_ref(); // 分配新临时变量
                s = s + get_ref() + " = eq " + unaryExp->get_ref_if_possible() + ", 0\n";
                t_val = to_string(!atoi(unaryExp->t_val.data()));
            }
            else if (unaryOp == "-")
            {
                s = unaryExp->Dump();
                alloc_ref(); // 分配新临时变量
                s = s + get_ref() + " = sub 0, " + unaryExp->get_ref_if_possible() + "\n";
                t_val = to_string(-atoi(unaryExp->t_val.data()));
            }
            else if (unaryOp == "+") // 不产生新指令，照搬之前的指令
            {
                s = unaryExp->Dump();
                t_id = unaryExp->t_id;
                t_val = unaryExp->t_val;
            }
            t_type = unaryExp->t_type;
            break;
        default:
            cerr << "Parsing Error: in UnaryExp:=PrimaryExp|UnaryOp UnaryExp\n";
            assert(false);
        }
        return s;
    }
};

class MulExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> unaryExp; // 一元表达式
    // 产生式2
    string mulop; // 多元算符
    unique_ptr<BaseAST> mulExp;

    string Dump() override
    {
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            s = unaryExp->Dump();
            t_id = unaryExp->t_id;
            t_val = unaryExp->t_val;
            t_type = unaryExp->t_type;
            break;
        case 2:
            s = mulExp->Dump();    // 先求出多元式（左结合）
            s1 = unaryExp->Dump(); // 然后求出一元式

            t_type = mulExp->t_type;   // 没有考虑类型转换和检查
            alloc_ref(); // 分配新临时变量

            s = s + s1 +
                get_ref() + " = " + get_koopa_op(mulop) + " " + 
                mulExp->get_ref_if_possible() + ", " +
                unaryExp->get_ref_if_possible() + "\n";

            if (mulop == "*")
            {
                t_val = to_string(atoi(mulExp->t_val.data()) * atoi(unaryExp->t_val.data()));
            }
            else if (mulop == "/")
            {
                t_val = to_string(atoi(mulExp->t_val.data()) / atoi(unaryExp->t_val.data()));
            }
            else if (mulop == "%")
            {
                t_val = to_string(atoi(mulExp->t_val.data()) % atoi(unaryExp->t_val.data()));
            }
            break;
        default:
            assert(false);
        }
        return s;
    }
};

class AddExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> mulExp;
    // 产生式2
    string mulop; // 多元算符
    unique_ptr<BaseAST> addExp;

    string Dump() override
    {
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            s = mulExp->Dump();
            t_id = mulExp->t_id;
            t_val = mulExp->t_val;
            t_type = mulExp->t_type;
            break;
        case 2:
            s = addExp->Dump();
            s1 = mulExp->Dump();

            t_type = addExp->t_type; // 没有考虑类型转换和检查
            alloc_ref();

            s = s + s1 +
                get_ref() + " = " + get_koopa_op(mulop) + " " +
                addExp->get_ref_if_possible() + ", " +
                mulExp->get_ref_if_possible() + "\n";

            if (mulop == "+")
            {
                t_val = to_string(atoi(addExp->t_val.data()) + atoi(mulExp->t_val.data()));
            }
            else if (mulop == "-")
            {
                t_val = to_string(atoi(addExp->t_val.data()) - atoi(mulExp->t_val.data()));
            }

            break;
        default:
            assert(false);
        }
        return s;
    }
};

class RelExpAST : public BaseAST {
    public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> addExp;
    // 产生式2
    unique_ptr<BaseAST> relExp;
    string relop;

    string Dump() override {
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            s = addExp->Dump();
            t_id = addExp->t_id;
            t_val = addExp->t_val;
            t_type = addExp->t_type;
            break;
        case 2:
            s = relExp->Dump();
            s1 = addExp->Dump();

            t_type = relExp->t_type; // 没有考虑类型转换和检查
            alloc_ref();

            s = s + s1 +
                get_ref() + " = " + get_koopa_op(relop) + " " +
                relExp->get_ref_if_possible() + ", " +
                addExp->get_ref_if_possible() + "\n";

            if (relop == ">")
            {
                t_val = to_string(atoi(relExp->t_val.data()) > atoi(addExp->t_val.data()));
            }
            else if (relop == "<")
            {
                t_val = to_string(atoi(relExp->t_val.data()) < atoi(addExp->t_val.data()));
            }
            else if (relop == ">=")
            {
                t_val = to_string(atoi(relExp->t_val.data()) >= atoi(addExp->t_val.data()));
            }
            else if (relop == "<=")
            {
                t_val = to_string(atoi(relExp->t_val.data()) <= atoi(addExp->t_val.data()));
            }

            break;
        default:
            assert(false);
        }
        return s;
    }
};

class EqExpAST : public BaseAST {
    public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> relExp;
    // 产生式2
    unique_ptr<BaseAST> eqExp;
    string eqop;

    string Dump() override {
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            s = relExp->Dump();
            t_id = relExp->t_id;
            t_val = relExp->t_val;
            t_type = relExp->t_type;
            break;
        case 2:
            s = eqExp->Dump();
            s1 = relExp->Dump();

            t_type = eqExp->t_type; // 没有考虑类型转换和检查
            alloc_ref();

            s = s + s1 +
                get_ref() + " = " + get_koopa_op(eqop) + " " +
                eqExp->get_ref_if_possible() + ", " +
                relExp->get_ref_if_possible() + "\n";

            if (eqop == "==")
            {
                t_val = to_string(atoi(eqExp->t_val.data()) == atoi(relExp->t_val.data()));
            }
            else if (eqop == "!=")
            {
                t_val = to_string(atoi(eqExp->t_val.data()) != atoi(relExp->t_val.data()));
            }

            break;
        default:
            assert(false);
        }
        return s;
    }
};

class LAndExpAST : public BaseAST {
    public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> eqExp;
    // 产生式2
    unique_ptr<BaseAST> landExp;

    string Dump() override {
        string s, s1, s2, s3;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            s = eqExp->Dump();
            t_id = eqExp->t_id;
            t_val = eqExp->t_val;
            t_type = eqExp->t_type;
            break;
        case 2:
            s = landExp->Dump();
            s1 = eqExp->Dump();

            t_type = landExp->t_type; // 没有考虑类型转换和检查
            alloc_ref();
    // Koopa IR只支持按位与；a && b => %1 = ne a, 0; %2 = ne b, 0; %3 = and %1, %2; 
            s = s + s1 +
                get_ref() + " = ne " +
                landExp->get_ref_if_possible() + ", 0\n"; 
            s2 = get_ref();
            alloc_ref();
            s = s + get_ref() + " = ne " +
                eqExp->get_ref_if_possible() + ", 0\n";
            s3 = get_ref();
            alloc_ref();
            s = s + get_ref() + " = and " + s2 + ", " + s3 + "\n";
            t_val = to_string(atoi(landExp->t_val.data()) && atoi(eqExp->t_val.data()));

            break;
        default:
            assert(false);
        }
        return s;
    }
};

class LOrExpAST : public BaseAST {
    public:
    // 表示选择了哪个产生式
    uint32_t selection;
    // 产生式1
    unique_ptr<BaseAST> landExp;
    // 产生式2
    unique_ptr<BaseAST> lorExp;

    string Dump() override {
        string s, s1, s2, s3;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            s = landExp->Dump();
            t_id = landExp->t_id;
            t_val = landExp->t_val;
            t_type = landExp->t_type;
            break;
        case 2:
            s = lorExp->Dump();
            s1 = landExp->Dump();

            t_type = lorExp->t_type; // 没有考虑类型转换和检查
            alloc_ref();
    // Koopa IR只支持按位或; a || b => %1 = ne a, 0; %2 = ne b, 0; %3 = or %1, %2; 
            s = s + s1 +
                get_ref() + " = ne " +
                lorExp->get_ref_if_possible() + ", 0\n"; 
            s2 = get_ref();
            alloc_ref();
            s = s + get_ref() + " = ne " +
                landExp->get_ref_if_possible() + ", 0\n";
            s3 = get_ref();
            alloc_ref();
            s = s + get_ref() + " = or " + s2 + ", " + s3 + "\n";

            t_val = to_string(atoi(lorExp->t_val.data()) || atoi(landExp->t_val.data()));

            break;
        default:
            assert(false);
        }
        return s;
    }
};
// ...

#endif // AST_HPP