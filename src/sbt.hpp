// 符号表声明
#ifndef SBT_HPP
#define SBT_HPP

#include <vector>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <cassert>
using namespace std;

// 以下常量规定了Fe语言类型所使用的类型名称
const static string FE_TYPENAME_INT = "int";

const static int WIDTH_UNIT = 4; // 整型宽度单位

enum ValueType
{
    INT_VT
};
static unordered_map<string, int> TYPE_HASH;
inline static void initTypeHash(); // 初始化类型哈希

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

// 变量定义表
class VarSBTNode
{
public:
    int typeBlockId; // 类定义时所在块（对应TypeSBT的行）
    int typeId;      // 类定义时所在的类id（对应TypeSBT的列）
    int offset;      // 变量偏移量

    // int srcRowId;           // 对应Fe源代码行号
};

static vector<unordered_map<string, VarSBTNode> *> VAR_SBT;        // 行对应Block，哈希键：变量名
static bool findVarInSBT(int blockId, string name);                // 在VAR_SBT中查询变量声明是否存在
static void addVarToSBT(int blockId, ValueType type, string name); // 注册变量声明到VAR_SBT

// 常量定义表
class ConstSBTNode
{
public:
    int value;       // 暂时只支持整型值，因此用int即可
    int typeBlockId; // 类定义时所在块（对应TypeSBT的行）
    int typeId;      // 类定义时所在的类id（对应TypeSBT的列）
    int offset;      // 变量偏移量

    // int srcRowId;           // 对应Fe源代码行号
};

static vector<unordered_map<string, ConstSBTNode> *> CONST_SBT; // 行对应Block，哈希键：常量名
static bool findConstInSBT(int blockId, string name);
static void addConstToSBT(int blockId, string name, int initVal);
static string getConstValFromSBT(int blockId, string name); // 读取常量值

static int global_block_id = 0; // 块id计数器，每个块的id都不同
inline static void alloc_block(int blockId, int parentBlockId); // 为blockId分配所需的各类块空间
inline static int alloc_block_id(int parentBlockId);    // 分配产生一个唯一的blockId，并分配空间

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

static bool findVarInSBT(int blockId, string name)
{
    // VAR_SBT不存在该块，报错
    if (VAR_SBT.size() <= blockId)
    {
        cerr << "Block Undefined in VAR_SBT Error: " << blockId << endl;
        assert(false);
    }
    // 在该块中寻找变量声明
    if (VAR_SBT[blockId]->count(name))
    {
        return true;
    }
    // DFS: 在父块中寻找，根块没有父块
    if (blockId)
    {
        // 由于我们只有本块的id，因此要获取父块id先要得到本块的block
        return findVarInSBT(BLOCK_HASH.at(blockId)->parent->blockId, name);
    }
    return false;
}

static void addVarToSBT(int blockId, ValueType type, string name)
{
    // 判断该变量声明是否已经出现过
    if (findVarInSBT(blockId, name))
    {
        cerr << "Syntax Error: Variable Redefined " << blockId << " " << type << " " << name << endl;
        assert(false);
    }
    // 向该块添加变量声明
    VarSBTNode var;
    var.typeBlockId = 0; // 【后期增加类型时须修改】找到变量类型声明时所在的块id，目前只有int类型，所以blockId固定为0
    var.typeId = 0;      // 【后期增加类型时须修改】找到变量类型声明时所在的类id，目前只有int类型，所以typeId固定为0
    VAR_SBT[blockId]->insert(make_pair(name, var));
}

static bool findConstInSBT(int blockId, string name)
{
    // CONST_SBT不存在该块，报错
    if (CONST_SBT.size() <= blockId)
    {
        cerr << "Block Undefined in CONST_SBT Error: " << blockId << endl;
        assert(false);
    }
    // 在该块中寻找常量声明
    int cnt = CONST_SBT[blockId]->count(name);

    if (cnt)
    {
        return true;
    }
    cerr << "Unfound in CONST_SBT...\n";
    // DFS: 在父块中寻找，根块没有父块
    if (blockId)
    {
        cerr << "Finding in parent block: " << blockId << " " << BLOCK_HASH.at(blockId)->parent->blockId << endl;
        // 由于我们只有本块的id，因此要获取父块id先要得到本块的block
        return findConstInSBT(BLOCK_HASH.at(blockId)->parent->blockId, name);
    }
    return false;
}

static void addConstToSBT(int blockId, string name, int initVal)
{
    cerr << "Verifying ConstSBT... " << blockId << endl;
    // 判断该变量声明是否已经出现过
    if (findConstInSBT(blockId, name))
    {
        cerr << "Syntax Error: Const Redefined " << blockId << " " << name << " " << initVal << endl;
        assert(false);
    }
    cerr << "Verifying OK!\n";
    // 向该块添加变量声明
    ConstSBTNode con;
    con.typeBlockId = 0; // 【后期增加类型时须修改】找到变量类型声明时所在的块id，目前只有int类型，所以blockId固定为0
    con.typeId = 0;      // 【后期增加类型时须修改】找到变量类型声明时所在的类id，目前只有int类型，所以typeId固定为0
    con.value = initVal;
    CONST_SBT[blockId]->insert(make_pair(name, con));
    cerr << "AddOK!: " << CONST_SBT[blockId]->at(name).value << endl;
}

static inline string getConstValFromSBT(int blockId, string name)
{
    // CONST_SBT不存在该块，报错
    if (CONST_SBT.size() <= blockId)
    {
        cerr << "Block Undefined in CONST_SBT Error: " << blockId << endl;
        assert(false);
    }
    // 在该块中寻找常量声明时的值
    cerr << "finding const val...\n";
    auto findResult = CONST_SBT[blockId]->find(name);
    auto notFound = CONST_SBT[blockId]->end();
    if (findResult != notFound)
    {
        stringstream ss;
        ss << findResult->second.value;
        return ss.str();
    }
    cerr << "not found! finding parent...\n";
    // DFS: 在父块中寻找，根块没有父块
    if (blockId)
    {
        // 由于我们只有本块的id，因此要获取父块id先要得到本块的block
        return getConstValFromSBT(BLOCK_HASH.at(blockId)->parent->blockId, name);
    }
    cerr << "Syntax Error: Const Undefined! " << name << endl;
    assert(false);
    return "\0";
}

inline static void alloc_block(int blockId, int parentBlockId)
{
    cerr << "allocing space for and parent: " << blockId << " " << parentBlockId << endl;
    BLOCK_HASH.at(blockId) = new BlockNode(blockId, parentBlockId);
    cerr << "block hash size: " << BLOCK_HASH.size() << endl;
    if(blockId) cerr << "allocated space for and parent: " << blockId << " " << BLOCK_HASH.at(blockId)->parent->blockId << endl;
    // 【不能在该函数中new，对象指针会被析构，用匿名对象初始化才可以！】
    TYPE_SBT.emplace_back(new unordered_map<string, TypeSBTNode>());
    VAR_SBT.emplace_back(new unordered_map<string, VarSBTNode>());
    CONST_SBT.emplace_back(new unordered_map<string, ConstSBTNode>());
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