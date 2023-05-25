#include "koopavisitor.hpp"
#include "koopa.h"
#include <cassert>
#include <iostream>
#include <fstream>

using namespace std;

fstream fout;

void Visit(const koopa_raw_program_t &program, const char* filePath)
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
    for(size_t i = 0; i < program.funcs.len; ++i) {
        koopa_raw_function_t func = (koopa_raw_function_t)program.funcs.buffer[i];
        fout << " " << (func->name+1);
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
            assert(false);
        }
    }
}

// 访问函数
void Visit(const koopa_raw_function_t &func)
{
    // 执行一些其他的必要操作
    // ...
    
    fout << func->name+1 << ":\n"; 
    // Visit(func->params);    // raw slice类型
    // 访问所有基本块
    Visit(func->bbs);
}

void Visit(const koopa_raw_type_t& type) {
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
    case KOOPA_RVT_RETURN:
        // 访问 return 指令
        Visit(kind.data.ret);
        break;
    case KOOPA_RVT_INTEGER:
        // 访问 integer 指令
        Visit(kind.data.integer);
        break;
    case KOOPA_RVT_ALLOC:
        // 访问局部存储分配
        Visit(kind.data.store);
        break;
    
    default:
        // 其他类型暂时遇不到
        assert(false);
    }
}

void Visit(const koopa_raw_store_t &store) {
    Visit(store.dest); 
    fout << " ";
    Visit(store.value);
    fout << " \n";
}

void Visit(const koopa_raw_value_kind_t &kind) 
{
    fout << kind.tag << endl;
}

void Visit(const koopa_raw_return_t &ret)
{
    Visit(ret.value);
    fout << "  ret\n";
}

void Visit(const koopa_raw_integer_t &_int)
{
    fout << "  li a0, " << _int.value << "\n";
}

// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...