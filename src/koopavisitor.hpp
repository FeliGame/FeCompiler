#ifndef KOOPA_VISITOR_HPP
#define KOOPA_VISITOR_HPP

#include "koopa.h"


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


#endif // KOOPA_VISITOR_HPP