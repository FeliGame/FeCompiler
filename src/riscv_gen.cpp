#include "koopavisitor.hpp"
#include "koopa.h"
#include <cassert>
#include <iostream>
#include <fstream>

using namespace std;

fstream fout;

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
    if(imm == 0) {
        reg_prev_prev = reg_prev;
        reg_prev = "x0";
        return;
    }
    fout << "li\t" << alloc_reg() << ", " << imm << "\n";
}

// 配套try_save_reg使用
string reg_l, reg_r;
// 判断左右操作数是否为直接数，将直接数存入寄存器再进行计算
inline void try_save_reg(const koopa_raw_binary_t &bin_inst)
{
    if (bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER)
    {
        save_reg(bin_inst.lhs->kind.data.integer.value);
        reg_l = reg_prev;
    }
    else 
        reg_l = reg_prev;
    if (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER)
    {
        save_reg(bin_inst.rhs->kind.data.integer.value);
    }
    reg_r = reg_prev;
}

void Visit(const koopa_raw_program_t &program, const char *filePath)
{
    fout.open(filePath, ios_base::out);
    // 执行一些其他的必要操作
    // ...

    // 列出所有.text量
    fout << "  .text ";
    fout << endl;
    // 访问所有全局变量
    Visit(program.values);

    // 列出所有.globl量
    fout << "  .globl";
    for (size_t i = 0; i < program.funcs.len; ++i)
    {
        koopa_raw_function_t func = (koopa_raw_function_t)program.funcs.buffer[i];
        fout << " " << (func->name + 1);
    }
    fout << endl;
    // 访问所有函数
    Visit(program.funcs);
    fout << endl;

    fout.clear();
    fout.close();
}

void Visit(const koopa_raw_slice_t &slice)
{
    for (size_t i = 0; i < slice.len; ++i)
    {
        auto ptr = slice.buffer[i];
        // 根据 slice 的 kind 决定将 ptr 视作何种元素
        switch (slice.kind)
        {
        case KOOPA_RSIK_FUNCTION:
            // 访问函数
            Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
            break;
        case KOOPA_RSIK_BASIC_BLOCK:
            // 访问基本块
            Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
            break;
        case KOOPA_RSIK_VALUE:
            // 访问指令
            Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
            break;
        default:
            // 我们暂时不会遇到其他内容, 于是不对其做任何处理
            fout.close();
            assert(false);
        }
    }
}

// 访问函数
void Visit(const koopa_raw_function_t &func)
{
    // 执行一些其他的必要操作
    // ...

    fout << func->name + 1 << ":\n";
    // Visit(func->params);    // raw slice类型
    // 访问所有基本块
    Visit(func->bbs);
}

void Visit(const koopa_raw_type_t &type)
{
    fout << type->tag << endl;
}

// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb)
{
    // 执行一些其他的必要操作
    // ...
    // 访问所有指令
    // Visit(bb->params);
    Visit(bb->insts);
}

// 访问指令
void Visit(const koopa_raw_value_t &value)
{
    // 根据指令类型判断后续需要如何访问
    const auto &kind = value->kind;

    switch (kind.tag)
    {
    /// Integer constant.
    case KOOPA_RVT_INTEGER:
        Visit(kind.data.integer);
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
    // /// Memory load.
    // case KOOPA_RVT_LOAD:
    //     Visit(kind.data.load);
    //     break;
    // /// Memory store.
    // case KOOPA_RVT_STORE:
    //     Visit(kind.data.store);
    //     break;
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
        Visit(kind.data.binary);
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
    case KOOPA_RVT_RETURN:
        Visit(kind.data.ret);
        break;
    default:
        // 其他类型暂时遇不到
        cerr << "Inst Error: INST KIND: " << kind.tag << endl;
        fout.close();
        assert(false);
    }
}

void Visit(const koopa_raw_binary_t &bin_inst)
{
    // 对于l r全为常量，就直接计算l==r，最快，减少RISCV指令数量
    int32_t l = bin_inst.lhs->kind.data.integer.value;
    int32_t r = bin_inst.rhs->kind.data.integer.value;

    switch (bin_inst.op)
    {
    case KOOPA_RBO_EQ:  // riscv没有直接eq指令
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l == r);
        }
        else
        {
            try_save_reg(bin_inst);
            fout << "sub\t"<< alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "seqz\t" << reg_prev << ", " << reg_prev << "\n";
        }
        break;
    case KOOPA_RBO_NOT_EQ:  // riscv没有直接neq指令
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            save_reg(l != r);
        }
        else
        {
            try_save_reg(bin_inst);
            fout << "sub\t"<< alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
            fout << "snez\t" << reg_prev << ", " << reg_prev << "\n";
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
            fout << "sub\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "add\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "mul\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "sge\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "sgt\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "sle\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "slt\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "and\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
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
            fout << "or\t" << alloc_reg() << ", " << reg_l << ", " << reg_r << "\n";
        }
        break;
    default:
        cerr << "Inst Error: Unsupported op: " << bin_inst.op;
    }
}

void Visit(const koopa_raw_store_t &store)
{
}

void Visit(const koopa_raw_value_kind_t &kind)
{
    fout << kind.tag << endl;
}

void Visit(const koopa_raw_return_t &ret)
{
    // 如果只有直接数，则先写入寄存器，再mv（因为mv不能用直接数）
    if(reg_prev == "")
        save_reg(ret.value->kind.data.integer.value);
    fout << "mv\ta0, "<< reg_prev << "\n";
    fout << "ret\n";
}

void Visit(const koopa_raw_integer_t &_int)
{
    fout << _int.value << "\n";
}

// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...