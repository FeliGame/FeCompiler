// 符号表声明
#ifndef SBT_HPP
#define SBT_HPP

#include <vector>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <cassert>
using namespace std;

// 以下常量规定了Fe语言类型所使用的内置类型关键字
const static string FE_TYPENAME_INT = "int";
static string foundNameId;  // 变量赋值时找到的左值nameId

const static int WIDTH_UNIT = 4; // 整型宽度单位

enum ValueType
{
    INT_VT
};
static unordered_map<string, int> TYPE_HASH;
inline static void initTypeHash(); // 初始化类型哈希，其中加入内置类型【目前仅支持int】

enum Category
{
    BASIC_CT,
    CLASS_CT,
    POINTER_CT,
    ARRAY_CT
};

class BlockNode;
// 块哈希表
static unordered_map<int, BlockNode *> BLOCK_HASH;
inline static void initBlockHash();

// 块树，根块的blockId为0
class BlockNode
{
public:
    int blockId;
    BlockNode *parent;
    vector<BlockNode *> children;

    BlockNode(int blockId, int parentBlockId)
    {
        this->blockId = blockId;
        // 根块没有父节点
        if (blockId && (BLOCK_HASH.find(parentBlockId) != BLOCK_HASH.end()))
        {
            parent = BLOCK_HASH.at(parentBlockId);
            // 双向链接
            parent->children.emplace_back(this);
        }
        BLOCK_HASH.emplace(blockId, this);
    }
};

// 类型定义表
class TypeSBTNode
{
public:
    Category category;   // 类型所属大类
    int pClassDefBlock;  // 自定义类定义所在的块
    int baseTypeBlockId; // 基类所在块
    int baseTypeTypeId;  // 基类类id
    int array_size;      // （如果是数组类型）元素个数
    int width;           // 类型宽度

    // int srcRowId;           // 对应Fe源代码行号
};

static vector<unordered_map<string, TypeSBTNode> *> TYPE_SBT; // 行对应Block，哈希键：类型名
static void initTypeSBT();

// 常量定义表
class SBTNode
{
public:
    int const_val;       // 【暂仅支持整型】常量定义值（注意变量值无需放在这！）
    int typeBlockId; // 类定义时所在块（对应TypeSBT的行）
    int typeId;      // 类定义时所在的类id（对应TypeSBT的列）
    int offset;      // 变量偏移量【暂未使用】

    bool isConst;    // 是否为常量？

    // int srcRowId;           // 对应Fe源代码行号
};

static vector<unordered_map<string, SBTNode> *> SBT;              // 行对应Block，哈希键：常量名
// ast.hpp调用，检查某个ident名是否在块路径中存在
static bool findPureNameInSBT(int blockId, string pureName);
static bool findInSBT(int blockId, string name, bool findParent);                  // 用于检查是否某个名称是否被定义过
static void addConstToSBT(int blockId, string name, int initVal); // 声明常量时使用
static void addVarToSBT(int blockId, string name);                // 声明变量时使用
static SBTNode& getNodeFromSBT(int blockId, string name);            // 读取常量值

static int global_block_id = 0;                                 // 块id计数器，每个块的id都不同
inline static void alloc_block(int blockId, int parentBlockId); // 为blockId分配所需的各类块空间
inline static int alloc_block_id(int parentBlockId);            // 分配产生一个唯一的blockId，并分配空间
inline static string getNameId(int blockId, string name);       // 将名字和blockId缝合
inline static string truncIdFromName(string nameId);            // 将缝合的Name_BlockId分离出Name来

inline static string getNameId(int blockId, string name) {
    stringstream ss;
    ss << blockId;
    return name + "_" + ss.str();
}

inline static string truncIdFromName(string nameId) {
    int splitIndex = nameId.find_last_of('_');
    string result = nameId.substr(0, splitIndex);
    cerr << "split: " << result << endl;
    return result;
}

inline static void initTypeHash()
{
    TYPE_HASH.emplace(make_pair(FE_TYPENAME_INT, 0));
}

// 注册基本类型（目前只有int类型）到TYPE_SBT
inline static void initTypeSBT()
{
    TypeSBTNode t_int;
    t_int.category = BASIC_CT;
    t_int.width = WIDTH_UNIT;
    unordered_map<string, TypeSBTNode> block;
    block.insert(make_pair(FE_TYPENAME_INT, t_int));
    TYPE_SBT.emplace_back(&block);
}

static bool findPureNameInSBT(int blockId, string pureName)
{
    string nameId = getNameId(blockId, pureName);
    // SBT不存在该块，报错
    if (SBT.size() <= blockId)
    {
        cerr << "Block Undefined in SBT Error: " << blockId << endl;
        assert(false);
    }
    // 在该块中寻找常量声明
    int cnt = SBT[blockId]->count(nameId);
    if (cnt)
    {
        cerr << "FOUND: " << nameId << endl;
        foundNameId = nameId;
        return true;
    }
    cerr << "Unfound nameId in SBT... " << nameId << endl;
    // DFS: 在父块中寻找，根块没有父块
    if (blockId)
    {
        cerr << "Finding pure name in parent block: " << blockId << " " << BLOCK_HASH.at(blockId)->parent->blockId << endl;
        // 由于我们只有本块的id，因此要获取父块id先要得到本块的block
        return findPureNameInSBT(BLOCK_HASH.at(blockId)->parent->blockId, pureName);
    }
    return false;
}

static bool findInSBT(int blockId, string name, bool findParent)
{
    // SBT不存在该块，报错
    if (SBT.size() <= blockId)
    {
        cerr << "Block Undefined in SBT Error: " << blockId << endl;
        assert(false);
    }
    // 在该块中寻找常量声明
    int cnt = SBT[blockId]->count(name);

    if (cnt)
    {
        cerr << "FOUND: " << name << endl;
        return true;
    }
    cerr << "Unfound name in SBT... " << name << endl;
    // DFS: 在父块中寻找，根块没有父块
    if (findParent && blockId)
    {
        cerr << "Finding in parent block: " << blockId << " " << BLOCK_HASH.at(blockId)->parent->blockId << endl;
        // 由于我们只有本块的id，因此要获取父块id先要得到本块的block
        return findInSBT(BLOCK_HASH.at(blockId)->parent->blockId, name, true);
    }
    return false;
}

static void addConstToSBT(int blockId, string name, int initVal)
{
    cerr << "Verifying Const... " << blockId << endl;
    // 判断该变量声明是否已经出现过
    if (findInSBT(blockId, name, false))
    {
        cerr << "Syntax Error: Const Redefined " << blockId << " " << name << " " << initVal << endl;
        assert(false);
    }
    cerr << "Verifying OK!\n";
    // name = name
    SBTNode con;
    con.isConst = true;
    con.typeBlockId = 0; // 【后期增加类型时须修改】找到变量类型声明时所在的块id，目前只有int类型，所以blockId固定为0
    con.typeId = 0;      // 【后期增加类型时须修改】找到变量类型声明时所在的类id，目前只有int类型，所以typeId固定为0
    con.const_val = initVal;
    SBT[blockId]->insert(make_pair(name, con));
    cerr << "AddOK!: " << name << " " << SBT[blockId]->at(name).const_val << endl;
}

static void addVarToSBT(int blockId, string name)
{
    cerr << "Verifying Var... " << blockId << endl;
    // 判断该变量声明是否已经出现过
    if (findInSBT(blockId, name, false))
    {
        cerr << "Syntax Error: Var Redefined " << blockId << " " << name << endl;
        assert(false);
    }
    cerr << "Verifying OK!\n";
    // name = name
    SBTNode con;
    con.isConst = false;
    con.typeBlockId = 0; // 【后期增加类型时须修改】找到变量类型声明时所在的块id，目前只有int类型，所以blockId固定为0
    con.typeId = 0;      // 【后期增加类型时须修改】找到变量类型声明时所在的类id，目前只有int类型，所以typeId固定为0
    con.const_val = 0;      // 默认初始化值设为0
    SBT[blockId]->insert(make_pair(name, con));
    cerr << "AddOK!: " << name << endl;
}

static inline SBTNode& getNodeFromSBT(int blockId, string pureName)
{
    string nameId = getNameId(blockId, pureName);
    // SBT不存在该块，报错
    if (SBT.size() <= blockId)
    {
        cerr << "Block Undefined in SBT Error: " << blockId << endl;
        assert(false);
    }
    // 在该块中寻找常量声明时的值
    auto findResult = SBT[blockId]->find(nameId);
    auto notFound = SBT[blockId]->end();
    if (findResult != notFound)
    {
        cerr << "found node: " << nameId << endl;
        return findResult->second;
    }
    cerr << "not found: " << nameId << ", finding parent...\n";
    // DFS: 在父块中寻找，根块没有父块
    if (blockId)
    {
        // 由于我们只有本块的id，因此要获取父块id先要得到本块的block
        return getNodeFromSBT(BLOCK_HASH.at(blockId)->parent->blockId, pureName);
    }
    cerr << "Syntax Error: Const or val Undefined! " << nameId << endl;
    assert(false);
}

inline static void alloc_block(int blockId, int parentBlockId)
{
    cerr << "allocing space for and parent: " << blockId << " " << parentBlockId << endl;
    BLOCK_HASH.at(blockId) = new BlockNode(blockId, parentBlockId);
    cerr << "block hash size: " << BLOCK_HASH.size() << endl;
    if (blockId)
        cerr << "allocated space for and parent: " << blockId << " " << BLOCK_HASH.at(blockId)->parent->blockId << endl;
    // 【不能在该函数中new，对象指针会被析构，用匿名对象初始化才可以！】
    TYPE_SBT.emplace_back(new unordered_map<string, TypeSBTNode>());
    SBT.emplace_back(new unordered_map<string, SBTNode>());
}

inline static void initBlockHash()
{
    auto block = BlockNode(0, 0);
    alloc_block(0, 0);
}

inline static int alloc_block_id(int parentBlockId)
{
    int blockId = ++global_block_id;
    // 如果没有该块，添加该块
    if (!BLOCK_HASH.count(blockId))
    {
        alloc_block(blockId, parentBlockId);
    }
    return blockId;
}

#endif // SBT_HPP