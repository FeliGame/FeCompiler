#include "koopavisitor.hpp"
#include "koopa.h"
#include <cassert>
#include <iostream>
#include <fstream>

using namespace std;

fstream fout;

string reg_prev; // 上一个用到的寄存器

// 只需要记录寄存器是否使用，无需存储具体值
bool reg_t_used[7];
string alloc_reg_t()
{
    for (size_t i = 0; i < 8; ++i)
    {
        if (reg_t_used[i])
            continue;
        reg_t_used[i] = true;
        reg_prev = "t" + to_string(i);
        return reg_prev;
    }
    cerr << "Reg Error: NO FREE REG_T!\n";
    fout.close();
    assert(false);
}

// 只需要记录寄存器是否使用，无需存储具体值
bool reg_a_used[8];
string alloc_reg_a()
{
    for (size_t i = 0; i < 8; ++i)
    {
        if (reg_a_used[i])
            continue;
        reg_a_used[i] = true;
        reg_prev = "a" + to_string(i);
        return reg_prev;
    }
    cerr << "Reg Error: NO FREE REG_A!\n";
    fout.close();
    assert(false);
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
    // fout << "op: ";
    string reg_r; // 可能用到的右值寄存器
    switch (bin_inst.op)
    {
    case KOOPA_RBO_EQ:
        // eq l r 翻译为如下RISCV
        // 判断l r是常数或寄存器？
        // 对于l r全为常量
        if ((bin_inst.lhs->kind.tag == KOOPA_RVT_INTEGER) &&
            (bin_inst.rhs->kind.tag == KOOPA_RVT_INTEGER))
        {
            int32_t l = bin_inst.lhs->kind.data.integer.value;
            int32_t r = bin_inst.rhs->kind.data.integer.value;
            // 都是常数，就直接计算l==r，最快，减少RISCV指令数量
            fout << "li\t" << alloc_reg_t() << ", " << (l == r) << "\n";
        }
        else
        {
            // ...
            cerr << "Inst Error: in EQ: Undefined branch!\n";
            cerr << bin_inst.lhs->kind.tag << " " << bin_inst.rhs->kind.tag << endl;
            fout.close();
            assert(false);
        }
        break;
    case KOOPA_RBO_SUB:
        // 一元操作符运算
        reg_r = reg_prev; // 右值是原来的寄存器名称
        fout << "sub\t" << alloc_reg_t() << ", x0, " << reg_r << "\n";
        break;
    default:
        cerr << "Inst Error: Unsupported op!";
        fout << bin_inst.op;
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
    string reg_r = reg_prev;
    fout << "mv\t" << alloc_reg_a() << ", " << reg_prev << "\n";
    // Visit(ret.value);
    fout << "ret\n";
}

void Visit(const koopa_raw_integer_t &_int)
{
    fout << " integer: " << _int.value << "\n";
}

// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...