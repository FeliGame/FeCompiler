#ifndef AST_HPP
#define AST_HPP

#include <memory>
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "sbt.hpp"

using namespace std;
// 不能全局声明变量和函数！！！

// 全局临时变量使用记录
static bool global_t_id[1024];
// 跳过不可达代码段的字符串内记号（该记号之后的部分不会输出到IR）
static const string SKIP_UNREACHABLE = "`";

// 是否启用调试信息（cerr输出AST结构）
static bool debugMode = true;
static int branch_cnt = 0;  // 还有未跳转出去的块吗？（用于某些没有分支控制的ret语句）
static bool block_end = false; // 基本块是否结束的标记
static int basic_block_tag_id = 0;
// 分配tag_cnt个全局基本块标签（用于分支、循环、跳转语句）
static vector<string> alloc_basic_block_tags(int tag_cnt)
{
    stringstream ss;
    vector<string> result;
    while (tag_cnt--)
    {
        ss << (basic_block_tag_id++);
        result.push_back(("%L" + ss.str()));
        ss.str(""); // flush和clear方法不能清空流中内容
    }
    return result;
}

// 输出块末的jump，如果已经遇到了ret，就不用输出
static string get_block_end_ir(const vector<string> &tags, int index)
{
    // 前面已经遇到ret语句了，因此没有必要再输出jump
    if (block_end)
    {
        block_end = false; // ret语句已过
        return "\n\n";
    }
    block_end = true;
    return "jump " + tags[index] + "\n\n";
}
// 所有 AST 的基类
class BaseAST
{
public:
    // 继承属性
    int32_t depth = 0; // AST树深度（根节点深度为0）
    int t_type;        // 【表达式】计算结果的类型
    int blockId;       // 块ID

    // 综合属性
    int32_t t_id = -1; // 【表达式】计算结果的变量名序号
    // 【可优化】如果一个式子规约到出现第一条IR时，此后无需再计算t_val值，除非想优化
    string r_val;         // 【表达式】计算结果的右值，主要是因为部分指令对值没有影响，因此需要通过它来继承
    bool isConst = false; // 是否为常量或字面量，可以进行【常量合并】优化
    string ident;         //  【变量/常量名】

    virtual ~BaseAST() = default;
    // Dump translates the AST into Koopa IR string
    virtual string Dump() = 0;

    // 同步子节点的综合属性：t_id r_val t_type isConst isRet
    inline void syncProps(const unique_ptr<BaseAST> &child)
    {
        t_id = child->t_id;
        r_val = child->r_val;
        t_type = child->t_type;
        isConst = child->isConst;
        ident = child->ident;
    }

    // 输出AST
    void debug(const string &nodeName, const unique_ptr<BaseAST> &child)
    {
        if (!debugMode)
            return;
        if (child != nullptr)
            child->depth = depth + 1;
        for (int i = 0; i < depth; ++i)
        {
            cerr << "|";
        }
        cerr << nodeName << endl;
    }

    // 从全局t_id分配新的t_id给当前节点
    void alloc_ref()
    {
        for (size_t i = 0; i < 1024; ++i)
        {
            if (!global_t_id[i])
            {
                t_id = i;
                global_t_id[i] = true;
                return;
            }
        }
    }
    // 释放（尽可能用少的t_id，减少寄存器浪费空间）
    inline void free_ref(int32_t t_id)
    {
        if (t_id == -1)
            return;
        global_t_id[t_id] = false;
    }

    // 获取引用名（常作为左值）
    inline string get_ref()
    {
        return "%" + to_string(t_id);
    }

    // 若为常量或字面量，返回值；否则返回临时变量名ref
    inline string get_val_if_possible()
    {
        // 常量直接返回
        if (isConst)
            return r_val;
        if (t_id == -1)
        {
            cerr << "Unallocated pointer...\n";
            assert(false);
        }
        // 都不是，只能返回临时缓冲内容
        return get_ref();
    }

    // exp代表的值是指针
    inline string loadIfisPointer()
    {
        cerr << "loading pointer... " << ident << endl;
        // 在这次调用中将会改变foundNameId
        if (!isConst && findPureNameInSBT(blockId, truncIdFromName(ident)))
        {
            alloc_ref();
            ident = foundNameId;
            return get_ref() + " = load @" + ident + "\n";
        }
        return "";
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
        initTypeHash();
        initBlockHash();
        initTypeSBT();
        depth = 0;
        debug("comp_unit", func_def);
        string s = func_def->Dump();
        return s.substr(0, s.find_first_of(SKIP_UNREACHABLE));  // 不可达部分不需要输出
    }
};

// 声明常量或变量
class DeclAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> const_decl;
    unique_ptr<BaseAST> var_decl;

    string Dump() override
    {
        string s;
        isConst = false; // 声明语句不是右值
        switch (selection)
        {
        case 1:
            debug("decl", const_decl);
            const_decl->blockId = blockId;
            s = const_decl->Dump();
            syncProps(const_decl);
            break;
        case 2:
            debug("decl", var_decl);
            var_decl->blockId = blockId;
            s = var_decl->Dump();
            syncProps(var_decl);
            break;
        default:
            assert(false);
        }
        return s;
    }
};

// 常量声明
class ConstDeclAST : public BaseAST
{
public:
    unique_ptr<BaseAST> b_type;
    unique_ptr<BaseAST> const_defs;

    string Dump() override
    {
        debug("const_decl", const_defs);
        isConst = false; // 声明语句不是右值
        b_type->blockId = const_defs->blockId = blockId;
        string s = b_type->Dump();
        const_defs->t_type = b_type->t_type; // 继承（左->右）
        s += const_defs->Dump();
        return s;
    }
};
/*

*/

class ConstDefsAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> const_defs;
    unique_ptr<BaseAST> const_def;

    string Dump() override
    {
        debug("const_defs", const_def);
        isConst = false; // 定义语句不是右值
        string s;
        switch (selection)
        {
        case 1:
            const_defs->t_type = const_def->t_type = t_type; // 继承（父->子）
            const_defs->blockId = const_def->blockId = blockId;
            s = const_defs->Dump() + const_def->Dump();
            break;
        case 2:
            const_def->t_type = t_type; // 继承（父->子）
            const_def->blockId = blockId;
            s = const_def->Dump();
            break;
        default:
            assert(false);
        }
        return s;
    }
};

// 声明时的类型
class BTypeAST : public BaseAST
{
public:
    string type;

    string Dump() override
    {
        debug("b_type", nullptr);
        t_type = TYPE_HASH.at(type);
        return "";
    }
};

// 常量定义
class ConstDefAST : public BaseAST
{
public:
    unique_ptr<BaseAST> const_init_val;

    string Dump() override
    {
        debug("const_def", const_init_val);
        string s;
        ident = getNameId(blockId, ident);
        const_init_val->t_type = t_type; // 继承（父->子）
        const_init_val->blockId = blockId;
        s = const_init_val->Dump();
        addConstToSBT(blockId, ident, atoi(const_init_val->r_val.data())); // t_type已经从父亲那里继承
        // 常量本质上和字面量没有区别，可以直接参与运算
        syncProps(const_init_val);
        return s;
    }
};

// 声明常量或变量
class ConstInitValAST : public BaseAST
{
public:
    unique_ptr<BaseAST> const_exp;

    string Dump() override
    {
        debug("const_init_val", const_exp);
        const_exp->t_type = t_type; // 继承（父->子）
        const_exp->blockId = blockId;
        string s = const_exp->Dump();
        syncProps(const_exp);
        return "";
    }
};

// 声明变量
class VarDeclAST : public BaseAST
{
public:
    unique_ptr<BaseAST> b_type;
    unique_ptr<BaseAST> var_defs;

    string Dump() override
    {
        debug("var_decl", var_defs);
        isConst = false; // 声明语句不是右值
        b_type->blockId = var_defs->blockId = blockId;
        string s = b_type->Dump();
        var_defs->t_type = b_type->t_type; // 继承（左->右）
        s += var_defs->Dump();
        return s;
    }
};

class VarDefsAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> var_defs;
    unique_ptr<BaseAST> var_def;

    string Dump() override
    {
        debug("var_defs", var_def);
        isConst = false; // 定义语句不是右值
        string s;
        switch (selection)
        {
        case 1:
            var_defs->t_type = var_def->t_type = t_type; // 继承（父->子）
            var_defs->blockId = var_def->blockId = blockId;
            s = var_defs->Dump() + var_def->Dump();
            break;
        case 2:
            var_def->t_type = t_type; // 继承（父->子）
            var_def->blockId = blockId;
            s = var_def->Dump();
            break;
        default:
            assert(false);
        }
        return s;
    }
};

// 定义变量
class VarDefAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> init_val;

    string Dump() override
    {
        debug("var_def", nullptr);
        ident = getNameId(blockId, ident);
        addVarToSBT(blockId, ident);               // 添加变量声明
        string s = "@" + ident + " = alloc i32\n"; // 【暂时只支持int类型变量分配】
        if (selection == 1)
        {
            s += ("store 0, @" + ident + "\n");
        }
        if (selection == 2)
        {
            debug("var_def", init_val);
            init_val->blockId = blockId;
            s += (init_val->Dump() + init_val->loadIfisPointer());
            s += ("store " + init_val->get_val_if_possible() + ", @" + ident + "\n");
        }
        return s;
    }
};

// 初始化值
class InitValAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;

    string Dump() override
    {
        debug("init_val", exp);
        exp->blockId = blockId;
        string s = exp->Dump();
        syncProps(exp);
        return s;
    }
};

class FuncDefAST : public BaseAST
{
public:
    unique_ptr<BaseAST> func_type; // 返回值类型
    unique_ptr<BaseAST> block;     // 函数体

    string Dump() override
    {
        debug("func_def", func_type);
        debug("func_def", block);
        block->blockId = blockId; // 父块的id传下去（C语言中，由于不允许嵌套函数，所以FuncDef->blockId = 0）
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
        debug("func_type", nullptr);
        string s = "";
        if (type == "int")
            s = "i32";
        return s;
    }
};

class BlockAST : public BaseAST
{
public:
    unique_ptr<BaseAST> block_items;
    bool isNull;

    string Dump() override
    {
        string s;
        if (!isNull)
        {
            debug("block", block_items);
            blockId = alloc_block_id(blockId); // 传入参数为父块id，从这里转变成了该节点本身id
            block_items->blockId = blockId;
            s = block_items->Dump(); // 父->子
        }
        return s;
    }
};

class BlockItemsAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> block_item;
    unique_ptr<BaseAST> block_items;

    string Dump() override
    {
        // 层次不会加深，故不用debug
        string s;
        switch (selection)
        {
        case 1:
            block_items->blockId = block_item->blockId = blockId; // 父->子
            s = block_items->Dump() + block_item->Dump();
            break;
        case 2:
            block_item->blockId = blockId; // 父->子
            s = block_item->Dump();
            break;
        default:
            assert(false);
        }
        return s;
    }
};

// 块中可以是声明，也可以是表达式
class BlockItemAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> decl;
    unique_ptr<BaseAST> stmt;

    string Dump() override
    {
        string s;
        switch (selection)
        {
        case 1:
            debug("block_item", decl);
            decl->blockId = blockId; // 父->子
            s = decl->Dump();
            syncProps(decl);
            break;
        case 2:
            debug("block_item", stmt);
            stmt->blockId = blockId; // 父->子
            s = stmt->Dump();
            syncProps(stmt);
            break;
        default:
            assert(false);
        }
        return s;
    }
};

class StmtAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> ms_ums;

    string Dump() override
    {
        debug("stmt", nullptr);
        ms_ums->blockId = blockId;
        string s = ms_ums->Dump();
        return s;
    }
};

class MSAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> l_val;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> block;
    unique_ptr<BaseAST> lorExp;
    unique_ptr<BaseAST> ms;
    unique_ptr<BaseAST> else_ms;

    string Dump() override
    {
        debug("ms", nullptr);
        string s;
        string l_id;         // 左值的正确变量名
        vector<string> tags; // if-else语句需要的基本块标签
        switch (selection)
        {
        case 1:
            l_val->blockId = exp->blockId = blockId; // 父->子
            s = l_val->Dump();
            findPureNameInSBT(blockId, truncIdFromName(l_val->ident));
            l_id = foundNameId;
            // 字面量/常量不能作为左值
            if (l_val->isConst)
            {
                cerr << "Syntax Error: Const val isn't a left val!\n";
                assert(false);
            }
            // 变量赋值语句
            else
            {
                s += exp->Dump() + exp->loadIfisPointer();                          // 计算右值的结果
                s += ("store " + exp->get_val_if_possible() + ", @" + l_id + "\n"); // 注意要赋值给左值变量标签
            }
            break;
        case 2:
            exp->blockId = blockId; // 父->子
            s = exp->Dump();
            break;
        case 3:
            block->blockId = blockId; // 父->子
            s = block->Dump();
            break;
        // IF '(' Exp ')' ms ELSE else_ms
        case 4:
            exp->blockId = ms->blockId = else_ms->blockId = blockId;
            s = exp->Dump(); // 判断条件
            tags = alloc_basic_block_tags(3);
            ++branch_cnt;
            if (exp->t_id == -1 && !exp->isConst)
            {
                exp->alloc_ref();
                s += (exp->get_ref() + " = load @" + exp->ident + "\n"); // 对于exp为store指令的，由于没有返回值，要先加载到临时变量中
            }
            s += ("br " + exp->get_val_if_possible() + ", " + tags[0] + ", " + tags[1] + "\n\n");
            block_end = false;      // 新块开始
            s += (tags[0] + ":\n"); // exp 为真的基本块
            s += (ms->Dump() + get_block_end_ir(tags, 2));
            block_end = false;      // 新块开始
            s += (tags[1] + ":\n"); // exp 为假的基本块
            s += (else_ms->Dump() + get_block_end_ir(tags, 2));
            block_end = false;      // 新块开始
            s += (tags[2] + ":\n"); // 退出分支语句的基本块
            --branch_cnt;
            break;
        case 5:
            exp->blockId = blockId; // 父->子
            s = exp->Dump();
            // 返回的符号若为变量指针，则需要
            s += exp->loadIfisPointer();
            s += ("ret " + exp->get_val_if_possible() + "\n"); // 指令行
            block_end = true;
            // 如果ret不处于任何分支，那么此后不应该输出任何语句
            if(!branch_cnt) {
                s += ("}" + SKIP_UNREACHABLE);
            }
            break;
        default: // 对应只有;的空语句或return ;
            s = "";
        }
        return s;
    }
};

class UMSAST : public BaseAST
{
public:
    int selection;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> stmt;
    unique_ptr<BaseAST> ms;
    unique_ptr<BaseAST> ums;

    string Dump() override
    {
        debug("ms", nullptr);
        string s;
        string l_id;         // 左值的正确变量名
        vector<string> tags; // if-else语句需要的基本块标签
        switch (selection)
        {
        // IF '(' Exp ')' MS ELSE UMS
        case 1:
            exp->blockId = ms->blockId = ums->blockId = blockId;
            s = exp->Dump(); // 判断条件
            tags = alloc_basic_block_tags(3);
            ++branch_cnt;
            if (exp->t_id == -1 && !exp->isConst)
            {
                exp->alloc_ref();
                s += (exp->get_ref() + " = load @" + exp->ident + "\n"); // 对于exp为store指令的，由于没有返回值，要先加载到临时变量中
            }
            s += ("br " + exp->get_val_if_possible() + ", " + tags[0] + ", " + tags[1] + "\n\n");
            block_end = false;      // 新块开始
            s += (tags[0] + ":\n"); // exp 为真的基本块
            s += (ms->Dump() + get_block_end_ir(tags, 2));
            block_end = false;      // 新块开始
            s += (tags[1] + ":\n"); // exp 为假的基本块
            s += (ums->Dump() + get_block_end_ir(tags, 2));
            block_end = false;      // 新块开始
            s += (tags[2] + ":\n"); // 退出分支语句的基本块
            --branch_cnt;
            break;
        // IF '(' Exp ')' Stmt
        case 2:
            exp->blockId = stmt->blockId = blockId;
            s = exp->Dump();
            tags = alloc_basic_block_tags(2);
            ++branch_cnt;
            if (exp->t_id == -1 && !exp->isConst)
            {
                exp->alloc_ref();
                s += (exp->get_ref() + " = load @" + exp->ident + "\n"); // 对于exp为store指令的，由于没有返回值，要先加载到临时变量中
            }
            s += ("br " + exp->get_val_if_possible() + ", " + tags[0] + ", " + tags[1] + "\n\n");
            block_end = false;      // 新块开始
            s += (tags[0] + ":\n"); // exp 为真的基本块
            s += (stmt->Dump() + get_block_end_ir(tags, 1));
            block_end = false;      // 新块开始
            s += (tags[1] + ":\n"); // exp为假/退出分支语句的基本块
            --branch_cnt;
            break;
        default:
            cerr << "Parsing Error in: UMS\n";
            assert(false);
        }
        return s;
    }
};

// 狭义表达式（一元、二元）
class ExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    int selection;
    // unique_ptr<BaseAST> unaryExp; // 一元表达式
    // unique_ptr<BaseAST> addExp;   // 加法表达式
    unique_ptr<BaseAST> lorExp;

    string Dump() override
    {

        string s;
        switch (selection)
        {
        // case 1:
        //     unaryExp->depth = depth + 1;
        //     // 值不发生改变，因此原样复制
        //     s = unaryExp->Dump();
        //     syncProps(unaryExp);
        //     break;
        // case 2:
        //     addExp->depth = depth + 1;
        //     // 值不发生改变，因此原样复制
        //     s = addExp->Dump();
        //     syncProps(addExp);
        //     break;
        case 3:
            debug("exp", lorExp);
            lorExp->blockId = blockId; // 父->子
            lorExp->depth = depth + 1;
            // 值不发生改变，因此原样复制
            s = lorExp->Dump();
            syncProps(lorExp);
            break;
        default:
            assert(false);
        }
        return s;
    }
};

// 左值名称
class LValAST : public BaseAST
{
public:
    string Dump() override
    {
        debug("l_val", nullptr);
        ident = getNameId(blockId, ident);
        cerr << "lval id: " << ident << endl;
        auto node = getNodeFromSBT(blockId, truncIdFromName(ident));
        isConst = node.isConst; //  判断是否为常量，如果是，则可以在规约时合并常量
        if (isConst)
        {
            stringstream ss;
            ss << node.const_val;
            r_val = ss.str();
            cerr << "r_val = " << r_val << endl;
        }
        return "";
    }
};

// 主表达式（一元、二元、整数）
class PrimaryExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    int selection;
    // 产生式1
    unique_ptr<BaseAST> exp; // 表达式
    // 产生式2
    unique_ptr<BaseAST> l_val;
    // 产生式3
    uint32_t number; // 整数

    string Dump() override
    {
        string s = "";
        switch (selection)
        {
        case 1:
            debug("primary", exp);
            exp->blockId = blockId; // 父->子
            s = exp->Dump();
            syncProps(exp);
            break;
        case 2:
            debug("primary", l_val);
            l_val->blockId = blockId; // 父->子
            s = l_val->Dump();
            syncProps(l_val);
            break;
        case 3:
            t_id = -1;
            r_val = to_string(number);
            t_type = INT_VT;
            isConst = true; // 字面量（整型）是右值
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
    int selection;
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
            debug("unary", primaryExp);
            primaryExp->blockId = blockId; // 父->子
            s = primaryExp->Dump();
            syncProps(primaryExp);
            break;
        case 2:
            debug("unary", unaryExp);
            unaryExp->blockId = blockId; // 父->子
            s = unaryExp->Dump();        // 先计算子表达式的值
            if (unaryOp == "!")
            {
                if (!unaryExp->isConst)
                {
                    alloc_ref(); // 分配新临时变量
                    s = s + unaryExp->loadIfisPointer() + get_ref() + " = eq " + unaryExp->get_val_if_possible() + ", 0\n";
                }
                r_val = to_string(!atoi(unaryExp->r_val.data()));
            }
            else if (unaryOp == "-")
            {
                if (!unaryExp->isConst)
                {
                    alloc_ref(); // 分配新临时变量
                    s = s + unaryExp->loadIfisPointer() + get_ref() + " = sub 0, " + unaryExp->get_val_if_possible() + "\n";
                }
                r_val = to_string(-atoi(unaryExp->r_val.data()));
            }
            else if (unaryOp == "+") // 不产生新指令，照搬之前的指令
            {
                t_id = unaryExp->t_id;
                r_val = unaryExp->r_val;
            }
            else
            {
                cerr << "Parsing Error: Undefined unary op: " << unaryOp << endl;
                assert(false);
            }
            isConst = unaryExp->isConst;
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
    int selection;
    // 产生式1
    unique_ptr<BaseAST> unaryExp; // 一元表达式
    // 产生式2
    string mulop; // 多元算符
    unique_ptr<BaseAST> mulExp;

    string Dump() override
    {
        debug("mul", unaryExp);
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            unaryExp->blockId = blockId; // 父->子
            s = unaryExp->Dump();
            syncProps(unaryExp);
            break;
        case 2:
            debug("mul", mulExp);
            mulExp->blockId = unaryExp->blockId = blockId;       // 父->子
            s = mulExp->Dump() + mulExp->loadIfisPointer();      // 先求出多元式（左结合）
            s1 = unaryExp->Dump() + unaryExp->loadIfisPointer(); // 然后求出一元式
            isConst = mulExp->isConst & unaryExp->isConst;       // 仅当两个右值的运算结果才是右值
            t_type = mulExp->t_type;                             // 没有考虑类型转换和检查
            if (!isConst)
            {
                alloc_ref(); // 分配新临时变量

                s = s + s1 +
                    get_ref() + " = " + get_koopa_op(mulop) + " " +
                    mulExp->get_val_if_possible() + ", " +
                    unaryExp->get_val_if_possible() + "\n";
            }
            else // 是右值类型才能计算出结果
            {
                if (mulop == "*")
                {
                    r_val = to_string(atoi(mulExp->r_val.data()) * atoi(unaryExp->r_val.data()));
                }
                else if (mulop == "/")
                {
                    r_val = to_string(atoi(mulExp->r_val.data()) / atoi(unaryExp->r_val.data()));
                }
                else if (mulop == "%")
                {
                    r_val = to_string(atoi(mulExp->r_val.data()) % atoi(unaryExp->r_val.data()));
                }
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
    int selection;
    // 产生式1
    unique_ptr<BaseAST> mulExp;
    // 产生式2
    string mulop; // 多元算符
    unique_ptr<BaseAST> addExp;

    string Dump() override
    {
        debug("add", mulExp);
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            mulExp->blockId = blockId; // 父->子
            s = mulExp->Dump();
            syncProps(mulExp);
            break;
        case 2:
            debug("add", addExp);
            addExp->blockId = mulExp->blockId = blockId; // 父->子
            s = addExp->Dump() + addExp->loadIfisPointer();
            s1 = mulExp->Dump() + mulExp->loadIfisPointer();
            isConst = addExp->isConst & mulExp->isConst; // 仅当两个右值的运算结果才是右值
            t_type = addExp->t_type;                     // 没有考虑类型转换和检查

            if (!isConst)
            {
                alloc_ref();
                s = s + s1 +
                    get_ref() + " = " + get_koopa_op(mulop) + " " +
                    addExp->get_val_if_possible() + ", " +
                    mulExp->get_val_if_possible() + "\n";
            }
            else
            {
                if (mulop == "+")
                {
                    r_val = to_string(atoi(addExp->r_val.data()) + atoi(mulExp->r_val.data()));
                }
                else if (mulop == "-")
                {
                    r_val = to_string(atoi(addExp->r_val.data()) - atoi(mulExp->r_val.data()));
                }
            }
            break;
        default:
            assert(false);
        }
        return s;
    }
};

class RelExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    int selection;
    // 产生式1
    unique_ptr<BaseAST> addExp;
    // 产生式2
    unique_ptr<BaseAST> relExp;
    string relop;

    string Dump() override
    {
        debug("rel", addExp);
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            addExp->blockId = blockId; // 父->子
            s = addExp->Dump();
            syncProps(addExp);
            break;
        case 2:
            debug("rel", relExp);
            relExp->blockId = addExp->blockId = blockId; // 父->子
            s = relExp->Dump() + relExp->loadIfisPointer();
            s1 = addExp->Dump() + addExp->loadIfisPointer();
            isConst = relExp->isConst & addExp->isConst; // 仅当两个右值的运算结果才是右值
            t_type = relExp->t_type;                     // 没有考虑类型转换和检查

            if (!isConst)
            {
                alloc_ref();
                s = s + s1 +
                    get_ref() + " = " + get_koopa_op(relop) + " " +
                    relExp->get_val_if_possible() + ", " +
                    addExp->get_val_if_possible() + "\n";
            }
            else
            {
                if (relop == ">")
                {
                    r_val = to_string(atoi(relExp->r_val.data()) > atoi(addExp->r_val.data()));
                }
                else if (relop == "<")
                {
                    r_val = to_string(atoi(relExp->r_val.data()) < atoi(addExp->r_val.data()));
                }
                else if (relop == ">=")
                {
                    r_val = to_string(atoi(relExp->r_val.data()) >= atoi(addExp->r_val.data()));
                }
                else if (relop == "<=")
                {
                    r_val = to_string(atoi(relExp->r_val.data()) <= atoi(addExp->r_val.data()));
                }
            }
            break;
        default:
            assert(false);
        }
        return s;
    }
};

class EqExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    int selection;
    // 产生式1
    unique_ptr<BaseAST> relExp;
    // 产生式2
    unique_ptr<BaseAST> eqExp;
    string eqop;

    string Dump() override
    {
        debug("eq", relExp);
        string s, s1;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            relExp->blockId = blockId; // 父->子
            s = relExp->Dump();
            syncProps(relExp);
            break;
        case 2:
            debug("eq", eqExp);
            eqExp->blockId = relExp->blockId = blockId; // 父->子
            s = eqExp->Dump() + eqExp->loadIfisPointer();
            s1 = relExp->Dump() + relExp->loadIfisPointer();
            isConst = eqExp->isConst & relExp->isConst; // 仅当两个右值的运算结果才是右值
            t_type = eqExp->t_type;                     // 没有考虑类型转换和检查
            if (!isConst)
            {
                alloc_ref();
                s = s + s1 +
                    get_ref() + " = " + get_koopa_op(eqop) + " " +
                    eqExp->get_val_if_possible() + ", " +
                    relExp->get_val_if_possible() + "\n";
            }
            else
            {
                if (eqop == "==")
                {
                    r_val = (to_string(atoi(eqExp->r_val.data()) == atoi(relExp->r_val.data())));
                }
                else if (eqop == "!=")
                {
                    r_val = (to_string(atoi(eqExp->r_val.data()) != atoi(relExp->r_val.data())));
                }
            }

            break;
        default:
            assert(false);
        }
        return s;
    }
};

class LAndExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    int selection;
    // 产生式1
    unique_ptr<BaseAST> eqExp;
    // 产生式2
    unique_ptr<BaseAST> landExp;

    string Dump() override
    {
        debug("land", eqExp);
        string s, s1, s2, s3;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            eqExp->blockId = blockId;
            s = eqExp->Dump();
            syncProps(eqExp);
            break;
        case 2:
            debug("land", landExp);
            landExp->blockId = eqExp->blockId = blockId;
            s = landExp->Dump() + landExp->loadIfisPointer();
            s1 = eqExp->Dump() + eqExp->loadIfisPointer();
            isConst = landExp->isConst & eqExp->isConst; // 仅当两个右值的运算结果才是右值
            t_type = landExp->t_type;                    // 没有考虑类型转换和检查

            if (!isConst)
            {
                alloc_ref();
                // Koopa IR只支持按位与；a && b => %1 = ne a, 0; %2 = ne b, 0; %3 = and %1, %2;
                s = s + s1 +
                    get_ref() + " = ne " +
                    landExp->get_val_if_possible() + ", 0\n";
                s2 = get_ref();
                alloc_ref();
                s = s + get_ref() + " = ne " +
                    eqExp->get_val_if_possible() + ", 0\n";
                s3 = get_ref();
                alloc_ref();
                s = s + get_ref() + " = and " + s2 + ", " + s3 + "\n";
            }
            else
            {
                r_val = to_string(atoi(landExp->r_val.data()) && atoi(eqExp->r_val.data()));
            }
            break;
        default:
            assert(false);
        }
        return s;
    }
};

class LOrExpAST : public BaseAST
{
public:
    // 表示选择了哪个产生式
    int selection;
    // 产生式1
    unique_ptr<BaseAST> landExp;
    // 产生式2
    unique_ptr<BaseAST> lorExp;

    string Dump() override
    {
        debug("lor", landExp);
        string s, s1, s2, s3;
        switch (selection)
        {
        case 1:
            // 值不发生改变，因此原样复制
            landExp->blockId = blockId;
            s = landExp->Dump();
            syncProps(landExp);
            break;
        case 2:
            debug("lor", lorExp);
            lorExp->blockId = landExp->blockId = blockId;
            s = lorExp->Dump() + lorExp->loadIfisPointer();
            s1 = landExp->Dump() + landExp->loadIfisPointer();
            isConst = lorExp->isConst & landExp->isConst;
            t_type = lorExp->t_type; // 没有考虑类型转换和检查

            if (!isConst)
            {
                alloc_ref();
                // Koopa IR只支持按位或; a || b => %1 = ne a, 0; %2 = ne b, 0; %3 = or %1, %2;
                s = s + s1 +
                    get_ref() + " = ne " +
                    lorExp->get_val_if_possible() + ", 0\n";
                s2 = get_ref();
                alloc_ref();
                s = s + get_ref() + " = ne " +
                    landExp->get_val_if_possible() + ", 0\n";
                s3 = get_ref();
                alloc_ref();
                s = s + get_ref() + " = or " + s2 + ", " + s3 + "\n";
            }
            else
            {
                r_val = to_string(atoi(lorExp->r_val.data()) || atoi(landExp->r_val.data()));
            }
            break;
        default:
            assert(false);
        }
        return s;
    }
};

// 常量表达式
class ConstExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;

    string Dump() override
    {
        debug("const_exp", exp);
        exp->blockId = blockId;
        string s = exp->Dump();
        syncProps(exp);
        return s;
    }
};

#endif // AST_HPP