#ifndef KOOPA_VISITOR_HPP
#define KOOPA_VISITOR_HPP

#include "koopa.h"
#include <string>

using namespace std;

// 分配t寄存器组（t0~t6）
string alloc_reg();
// 分配a寄存器组（a0~a7）
string alloc_reg_a();
// 扫描IR string，以获取栈尺寸
void scanStackSize(string ir);
// 判断该value是否分配了栈，并返回已分配好的栈位置
inline int getStackPos(const koopa_raw_value_t&);

// DFS读取Raw Program

void VisitProgram(const koopa_raw_program_t&, const char*);
void VisitSlice(const koopa_raw_slice_t&);
void VisitFunc(const koopa_raw_function_t &func);
// void VisitType(const koopa_raw_type_t &type);
void VisitBlock(const koopa_raw_basic_block_t &bb);
void VisitValue(const koopa_raw_value_t &value);
// void VisitKind(const koopa_raw_value_kind_t &kind);
void VisitReturn(const koopa_raw_return_t&);
void VisitInt(const koopa_raw_integer_t&);

// 指令
void VisitLoad(const koopa_raw_load_t&);
void VisitStore(const koopa_raw_store_t&);
void VisitLoad(const koopa_raw_load_t&);

void VisitBin(const koopa_raw_binary_t&);
void VisitAlloc(const koopa_raw_global_alloc_t&);

#endif // KOOPA_VISITOR_HPP