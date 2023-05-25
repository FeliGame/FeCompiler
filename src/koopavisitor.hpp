#ifndef KOOPA_VISITOR_HPP
#define KOOPA_VISITOR_HPP

#include "koopa.h"
#include <string>

using namespace std;

// 分配t寄存器组（t0~t6）
string alloc_reg_t();
// 分配a寄存器组（a0~a7）
string alloc_reg_a();

// DFS读取Raw Program

void Visit(const koopa_raw_program_t&, const char*);
void Visit(const koopa_raw_slice_t&);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_type_t &type);
void Visit(const koopa_raw_basic_block_t &bb);
void Visit(const koopa_raw_value_t &value);
void Visit(const koopa_raw_value_kind_t &kind);
void Visit(const koopa_raw_return_t&);
void Visit(const koopa_raw_integer_t&);

void Visit(const koopa_raw_store_t&);
void Visit(const koopa_raw_binary_t&);


#endif // KOOPA_VISITOR_HPP