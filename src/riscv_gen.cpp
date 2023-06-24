#include "koopavisitor.hpp"
#include "koopa.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stack>
#include <set>
#include <unordered_map>

using namespace std;

fstream fout;

int stack_size; // 维护每个函数的栈空间长度（16字节对齐）

string reg_prev_prev = ""; // 上上个用到的寄存器
string reg_prev = "";      // 上一个用到的寄存器

// 只需要记录寄存器是否使用，无需存储具体值
bool reg_t_used[7];
// 只需要记录寄存器是否使用，无需存储具体值
bool reg_a_used[8];

string alloc_reg()
{
    for (size_t i = 0; i < 7; ++i)
    {
        if (reg_t_used[i])
            continue;
        reg_t_used[i] = true;
        reg_prev_prev = reg_prev;
        reg_prev = "t" + to_string(i);
        return reg_prev;
    }
    cerr << "Reg Error: NO FREE REG_T!\n";

    for (size_t i = 0; i < 8; ++i)
    {
        if (reg_a_used[i])
            continue;
        reg_a_used[i] = true;
        reg_prev_prev = reg_prev;
        reg_prev = "a" + to_string(i);
        return reg_prev;
    }
    cerr << "Reg Error: NO FREE REG_A!\n";
    fout.close();
    assert(false);
}

// 将直接数注册到寄存器中
inline void save_reg(int32_t imm)
{
    if (imm == 0)
    {
        reg_prev_prev = reg_prev;
        reg_prev = "x0";
        return;
    }
    fout << "li " << alloc_reg() << ", " << imm << "\n";
}

// 配套try_save_reg使用
string reg_l, reg_r;

int max_stack_pos = -4; // 分配的最大stack_pos
// 判断左右操作数是否为直接数，将直接数存入寄存器再进行计算
void try_save_reg(const koopa_raw_binary_t &bin_inst)
{
    if (bin_inst.lhs->kind.tag != KOOPA_RVT_INTEGER)
    {
        // fout << "lhs addr: " << bin_inst.lhs;
        fout << "lw t0, " << getStackPos(bin_inst.lhs) << "(sp)\n";
    }
    else
    {
        // save_reg(bin_inst.lhs->kind.data.integer.value);
        fout << "li t0, " << bin_inst.lhs->kind.data.integer.value << "\n";
    }
    reg_l = reg_prev;
    if (bin_inst.rhs->kind.tag != KOOPA_RVT_INTEGER)
    {
        // fout << "rhs addr: " << bin_inst.rhs;
        fout << "lw t1, " << getStackPos(bin_inst.rhs) << "(sp)\n";
    }
    else
    {
        fout << "li t1, " << bin_inst.rhs->kind.data.integer.value << "\n";
    }
    reg_r = reg_prev;
}

// 变量->栈位置的映射
unordered_map<string, int> id_map;

inline int getStackPos(const koopa_raw_value_t &value)
{
    string value_name;
    // %开头的变量没有value->name，使用value自身十六进制代号命名
    if (value->name == nullptr)
    {
        stringstream ss;
        ss << value;
        // fout << "allocing" << ss.str();
        value_name = ss.str();
    } else value_name = value->name;
    if (id_map.count(value_name))
    {
        // fout << "existing name: " << value_name << " " << id_map[value_name] << " ";
        return id_map[value_name];
    }
    else
    {
        // fout << "adding name: " << value_name << " " << id_map[value_name] << " ";
        max_stack_pos += 4;
        id_map.insert(make_pair(value_name, max_stack_pos));
        return max_stack_pos;
    }
}

void scanStackSize(string ir)
{
    stringstream ss(ir);
    string word;
    set<string> identifiers; // 存储所有变量（不重复）
    // 按空格切分ir
    while (ss >> word)
    {
        if (word[0] == '@' || word[0] == '%')
        {
            // 去掉末尾的逗号
            if (word.back() == ',')
            {
                word = word.substr(0, word.size() - 1);
            }
            identifiers.insert(word);
        }
    }
    cerr << "stack size: " << identifiers.size() << endl;
    stack_size = (identifiers.size() << 2); // 每个元素为4字节
}

void VisitProgram(const koopa_raw_program_t &program, const char *filePath)
{
    fout.open(filePath, ios_base::out);
    // 执行一些其他的必要操作
    // ...

    // 列出所有.text量
    fout << "  .text ";
    fout << endl;
    // 访问所有全局变量
    VisitSlice(program.values);

    // 列出所有.globl量
    fout << "  .globl";
    for (size_t i = 0; i < program.funcs.len; ++i)
    {
        koopa_raw_function_t func = (koopa_raw_function_t)program.funcs.buffer[i];
        fout << " " << (func->name + 1);
    }
    fout << endl;
    // 访问所有函数
    VisitSlice(program.funcs);
    fout << endl;

    fout.clear();
    fout.close();
}

void VisitSlice(const koopa_raw_slice_t &slice)
{
    for (size_t i = 0; i < slice.len; ++i)
    {
        auto ptr = slice.buffer[i];
        // 根据 slice 的 kind 决定将 ptr 视作何种元素
        switch (slice.kind)
        {
        case KOOPA_RSIK_FUNCTION:
            // 访问函数
            VisitFunc(reinterpret_cast<koopa_raw_function_t>(ptr));
            break;
        case KOOPA_RSIK_BASIC_BLOCK:
            // 访问基本块
            VisitBlock(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
            break;
        case KOOPA_RSIK_VALUE:
            // 访问指令
            VisitValue(reinterpret_cast<koopa_raw_value_t>(ptr));
            break;
        // case KOOPA_RSIK_TYPE:
        //     cerr << "Untold\n";
        //     assert(false);
        //     break;
        default:
            // 我们暂时不会遇到其他内容, 于是不对其做任何处理
            fout.close();
            assert(false);
        }
    }
}

// 访问函数
void VisitFunc(const koopa_raw_function_t &func)
{
    fout << func->name + 1 << ":\n";
    fout << "addi sp, sp, -" << stack_size << endl; // Prologue-为函数分配栈空间
    // Visit(func->params);    // raw slice类型
    // 访问所有基本块（一个函数可能含有多个基本块）
    VisitSlice(func->bbs);
}

// void VisitType(const koopa_raw_type_t &type)
// {
//     fout << type->tag << endl;
// }

// 访问基本块
void VisitBlock(const koopa_raw_basic_block_t &bb)
{
    // Visit(bb->params);
    VisitSlice(bb->insts);
}

// 访问指令
void VisitValue(const koopa_raw_value_t &value)
{
    // 根据指令类型判断后续需要如何访问
    const auto &kind = value->kind;

    switch (kind.tag)
    {
    /// Integer constant.
    case KOOPA_RVT_INTEGER:
        VisitInt(kind.data.integer);
        break;
    // /// Zero initializer.
    // case KOOPA_RVT_ZERO_INIT:
    //     break;
    // /// Undefined value.
    // case KOOPA_RVT_UNDEF:
    //     break;
    // /// Aggregate constant.
    // case KOOPA_RVT_AGGREGATE:
    //     Visit(kind.data.aggregate);
    //     break;
    // /// Function argument reference.
    // case KOOPA_RVT_FUNC_ARG_REF:
    //     Visit(kind.data.func_arg_ref);
    //     break;
    // /// Basic block argument reference.
    // case KOOPA_RVT_BLOCK_ARG_REF:
    //     Visit(kind.data.block_arg_ref);
    //     break;
    // /// Local memory allocation.
    // case KOOPA_RVT_ALLOC:
    //     break;
    // /// Global memory allocation.
    // case KOOPA_RVT_GLOBAL_ALLOC:
    //     Visit(kind.data.global_alloc);
    //     break;
    /// Memory load.
    case KOOPA_RVT_LOAD:
        fout << "lw t0, ";
        VisitLoad(kind.data.load);
        break;
    // /// Memory store.
    case KOOPA_RVT_STORE:
        VisitStore(kind.data.store);
        break;
    // /// Pointer calculation.
    // case KOOPA_RVT_GET_PTR:
    //     Visit(kind.data.get_ptr);
    //     break;
    // /// Element pointer calculation.
    // case KOOPA_RVT_GET_ELEM_PTR:
    //     Visit(kind.data.get_elem_ptr);
    //     break;
    /// Binary operation.
    case KOOPA_RVT_BINARY:
        VisitBin(kind.data.binary);
        break;
    // /// Conditional branch.
    // case KOOPA_RVT_BRANCH:
    //     Visit(kind.data.branch);
    //     break;
    // /// Unconditional jump.
    // case KOOPA_RVT_JUMP:
    //     Visit(kind.data.jump);
    //     break;
    // /// Function call.
    // case KOOPA_RVT_CALL:
    //     Visit(kind.data.call);
    //     break;
    /// Function return.
    case KOOPA_RVT_ALLOC:
        VisitAlloc(kind.data.global_alloc);
        break;
    case KOOPA_RVT_RETURN:
        if(kind.data.ret.value->kind.tag==KOOPA_RVT_INTEGER) {
            fout << "li a0, " << kind.data.ret.value->kind.data.integer.value << "\n";
        } else {
            fout << "lw a0, ";
            fout << getStackPos(kind.data.ret.value) << "(sp)\n";
        }
        VisitReturn(kind.data.ret);
        break;
    default:
        cerr << "Inst Error: INST KIND: " << kind.tag << endl;
        fout.close();
        assert(false);
    }
    // 除了调用函数时的栈操作外，所有有返回值指令结果都要存入内存
    if (value->ty->tag != KOOPA_RTT_UNIT && kind.tag != KOOPA_RVT_ALLOC)
    {
        // fout << "sw target: "<< value << " " << *(&value) << " ";
        fout << "sw t0, " << getStackPos(value) << "(sp)\n";
    }
}

void VisitInt(const koopa_raw_integer_t &_int)
{
    fout << _int.value << "\n";
}

void VisitLoad(const koopa_raw_load_t &load)
{
    fout << getStackPos(load.src) << "(sp)" << endl;
}

void VisitStore(const koopa_raw_store_t &store)
{
    // 操作数如果是直接数，要先加载到寄存器
    if (store.value->kind.tag == KOOPA_RVT_INTEGER)
        fout << "li t0, " << store.value->kind.data.integer.value << endl;
    else
        fout << "lw t0, " << getStackPos(store.value) << "(sp)\n";

    // 再将寄存器存入内存地址
    fout << "sw t0, " << getStackPos(store.dest) << "(sp)\n";
}

void VisitBin(const koopa_raw_binary_t &bin_inst)
{
    // 对于l r全为常量，就直接计算l==r，最快，减少RISCV指令数量
    int32_t l = bin_inst.lhs->kind.data.integer.value;
    int32_t r = bin_inst.rhs->kind.data.integer.value;

    switch (bin_inst.op)
    {
    case KOOPA_RBO_EQ: // riscv没有直接eq指令
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l == r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "sub " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            // fout << "seqz " << reg_prev << ", " << reg_prev << "\n";
            fout << "sub t0, t0, t1\n";
            fout << "seqz t0, t0\n";
        }
        break;
    case KOOPA_RBO_NOT_EQ: // riscv没有直接neq指令
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l != r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "sub " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            // fout << "snez " << reg_prev << ", " << reg_prev << "\n";
            fout << "sub t0, t0, t1\n";
            fout << "snez t0, t0\n";
        }
        break;
    case KOOPA_RBO_ADD:
        // 二元加法（一元的正号已在语义分析阶段过滤）
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l + r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "add " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "add t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_SUB:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l - r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "sub " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "sub t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_MUL: // 二元乘法
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l * r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "mul " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "mul t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_GE:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l >= r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "sge " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "sge t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_GT:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l > r);
        }
        else
        {
            try_save_reg(bin_inst);
            // sgt is pseudo instruct(RISCV-SPEC-20191213-P130)
            // fout << "sgt " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "sgt t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_LE:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l <= r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "sle " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "sle t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_LT:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l < r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "slt " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "slt t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_MOD:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l & r);
        }
        else
        {
            try_save_reg(bin_inst);
            fout << "rem t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_AND:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l & r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "and " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "and t0, t0, t1\n";
        }
        break;
    case KOOPA_RBO_OR:
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l | r);
        }
        else
        {
            try_save_reg(bin_inst);
            // fout << "or " << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "or t0, t0, t1\n";
        }
        break;
    default:
        cerr << "Inst Error: Unsupported op: " << bin_inst.op;
    }
}

// void VisitKind(const koopa_raw_value_kind_t &kind)
// {
//     fout << kind.tag << endl;
// }

void VisitAlloc(const koopa_raw_global_alloc_t &ret)
{
    // 没有操作
}

void VisitReturn(const koopa_raw_return_t &ret)
{
    // 如果只有直接数，则先写入寄存器，再mv（因为mv不能用直接数）
    // if (reg_prev == "")
    //     save_reg(ret.value->kind.data.integer.value);
    // 返回值类型为void
    // fout << "mv a0, " << reg_prev << "\n";
    fout << "addi sp, sp, " << stack_size << endl; // Epilogue-为函数清理栈空间
    fout << "ret\n";
}

// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...