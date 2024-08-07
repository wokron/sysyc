# 编译器设计文档

## 一、概述

本编译器以 SysY2022 语言源代码作为输入，以 QBE IR 作为中间代码，并以 RISC-V 汇编码作为输出。对于中间代码及汇编码，本编译器实现了一系列优化算法，包括常量折叠、死代码删除、控制流图优化、函数内联、GVN、窥孔优化等等。在目标代码生成部分，我们实现了基于线性扫描的寄存器分配算法。此外，本编译器也具有一定程度的错误处理，能够识别词法、语法和语义错误，并具有错误恢复能力。

在开发过程中，我们还编写了一系列脚本，如格式化脚本、测试脚本、部署脚本等以辅助项目的开发。

## 二、抽象语法树设计

我们使用了 flex 及 bison 实现了词法和语法分析阶段。在语义分析阶段后，我们基于文法定义了抽象语法树作为输出。

在设计抽象语法树时，我们重点考虑了语义分析的方便性，以及实现上的简洁性。由于比赛所使用的编译器支持 C++17 标准，因此在定义抽象语法树时我们选择使用 `std::variant`，而不是采用 `enum + union` 或继承实现。因为前者过于简陋，而后者又过于复杂。

`std::variant` 提供了在一个变量中包含多种类型的方法。因此能够很好的处理某一非终结符对应多条规则的情况。例如，在代码中，我们使用如下代码定义 Stmt 对应的语法树节点：

```cpp
struct AssignStmt;
struct ExpStmt;
struct BlockStmt;
struct IfStmt;
struct WhileStmt;
struct ControlStmt;
struct ReturnStmt;
using Stmt = std::variant<AssignStmt, ExpStmt, BlockStmt, IfStmt, WhileStmt,
                          ControlStmt, ReturnStmt>;
```

为了方便语义分析阶段的处理，我们也对文法进行了一定处理，定义了我们的抽象语法树。例如，对于表达式相关节点，我们没有采用原始的文法定义，而是将所有表达式使用统一的 `std::variant` 表示：

```cpp
using Exp =
    std::variant<BinaryExp, LValExp, CallExp, UnaryExp, CompareExp, Number>;
```

同时，各表达式子节点也统一使用 `Exp` 表示。

```cpp
struct BinaryExp {
    std::shared_ptr<Exp> left;
    enum {
        ADD,
        SUB,
        MULT,
        DIV,
        MOD,
    } op;
    std::shared_ptr<Exp> right;
};
```

通过这样的设计，在语义分析阶段我们得以定义统一的 `_visit_exp` 函数。并通过 `std::visit` 派发具体的解析函数。

```cpp
ExpReturn Visitor::visit_exp(const Exp &node) {
    return std::visit(
        overloaded{
            [this](const BinaryExp &node) { return visit_binary_exp(node); },
            [this](const LValExp &node) { return visit_lval_exp(node); },
            [this](const CallExp &node) { return visit_call_exp(node); },
            [this](const UnaryExp &node) { return visit_unary_exp(node); },
            [this](const CompareExp &node) { return visit_compare_exp(node); },
            [this](const Number &node) { return visit_number(node); },
        },
        node);
}
```

## 三、类型系统设计

由于文法定义中包含了 `int` 和 `float` 两个基本类型（以及 `void` 作为占位类型），以及数组和不完整的指针两个复合类型。为了方便语义分析阶段的处理，我们设计了较为完善的类型系统。

对于所有类型，我们定义了统一的抽象基类 `Type`。并在其中定义了 `==`、`!=` 运算的纯虚函数。

对于基本类型 `int` 和 `float`，为了方便处理，我们将其设定为单例。对于复合类型，可以将其视为其他类型的组合。因此在数组类型 `ArrayType` 和指针类型 `PointerType` 中，我们使用一个 `Type` 类型字段表示数组元素及指针引用的元素的类型。

对于类型，我们还定义了 `get_size` 方法以获取该类型在内存中的大小，定义了 `tostring` 方法以输出类型表达式等等。

最后，为了方便类型的构造，我们还定义了 `TypeBuilder` 类。该类提供的一系列方法，如 `TypeBuilder &in_array(int size)`、`TypeBuilder &in_ptr()` 能够方便地实现类型的构造。

## 四、语义分析设计

在语义分析阶段，我们采用访问者模式对抽象语法树进行遍历，并在遍历的过程中进行中间代码的构造。对于每一个抽象语法树节点，我们都定义了对应访问函数，例如 `void visit_decl(const Decl &node)`。

在遍历的过程中，我们进行了符号表的管理。我们定义了 `SymbolTable` 类用以表示单一作用域内的符号表。该类中包含一 `_father` 字段，指向更外一层的符号表。通过 `push_scope` 和 `pop_scope` 方法，我们能够实现作用域的管理。

```cpp
curr_scope = curr_scope->push_scope();
// do something in scope...
curr_scope = curr_scope->pop_scope();
```

符号表中我们也定义了 `add_symbol` 等方法以进行符号的管理。

在遍历抽象语法树时，我们大量使用了 `std::visitor`。该方法能够解析 `std::variant` 中存储的具体类型，并根据类型派发到对应的处理函数当中。

## 五、中间代码设计

SysY2022 的文法和语义较为简单，在这种情况下使用 LLVM 的中间代码便略显复杂。为了保持编译器的简洁性，我们决定选用 QBE 编译器的中间代码作为本编译器的中间代码。一方面，QBE IR 的结构较为简单，但足以满足 SysY2022 文法的生成；另一方面，与 LLVM 相同，QBE 作为已经存在的编译器，使用其中间代码能够更加方便地对中间代码的生成结果进行验证。

我们参考 QBE IR 设计了一系列数据结构。对于一个函数 `Function`，其中包含一系列基本块 `Block`，而基本块内存在三种类型的指令，一是基本块开头的一些 phi 指令；二是基本块中间的一些普通指令；最后是基本块末尾的一条跳转指令。由于这三类指令的功能各不相同，所以我们将这三种指令分别设定为三个不同的类型，并存储在不同的字段中。

与 LLVM 不同的是，我们的指令 `Inst` 并非 `Value`，而是一个包含了最多三个 `Value` 的结构体，并通过一个枚举区分不同的指令类型。如下所示

```cpp
struct Inst {
    InstType insttype;
    std::shared_ptr<Temp> to;
    std::shared_ptr<Value> arg[2];
    // ...
};
```

通过将 `Inst` 与 `Value` 区分，我们能够更加方便的在优化阶段对其进行操作。例如，在将栈上值移动到寄存器中时，我们可以直接对指令的字段进行一系列修改，并暂时破坏 SSA 形式。

## 六、优化设计

优化部分，我们实现了一系列优化算法。由于内容较多，这里并不一一详述。我们的大部分优化算法的实现来自于《SSA-based Compiler Design》这本书，同时我们也参考了较多其他的网上资料。

在本小节中，我们主要介绍本编译器中对于优化 Pass 的设计。所有的优化遍都继承自 `Pass` 基类。该类型中定义了纯虚方法 `bool run(ir::Module &module)`。对于优化范围不同的 Pass，我们还定义了不同的子类，如 `ModulePass`、`FunctionPass`、`BasicBlockPass` 等等。

特别的，通过变长参数模版，我们还设计了一种合并多个 Pass 的方法。我们定义了 `PassPipeline`，如下所示：

```cpp
template <typename... Ps> class PassPipeline : public Pass {
public:
    PassPipeline() {
        _passes = {new Ps()...};
    }

    ~PassPipeline() {
        for (auto &pass : _passes) {
            delete pass;
        }
    }

    bool run(ir::Module &module) override {
        bool changed = false;
        // just apply all passes in sequence
        for (auto &pass : _passes) {
            changed |= pass->run(module);
        }
        return changed;
    }

private:
    std::vector<Pass *> _passes;
};
```

我们可以通过该类型将多个 Pass 合并，例如我们将 SSA 构造阶段的三个 Pass 合并为单一的 Pass

```cpp
using SSAConstructPass =
    PassPipeline<opt::MemoryToRegisterPass, opt::PhiInsertingPass,
                 opt::VariableRenamingPass>;
```

最后，我们也使用 `PassPipeline` 定义了我们全部优化 Pass 的顺序：

```cpp
using Passes = opt::PassPipeline<
    opt::FillPredsPass, opt::SimplifyCFGPass, opt::FillPredsPass,
    opt::FillInlinePass, opt::FunctionInliningPass, opt::FillPredsPass,
    opt::FillReversePostOrderPass, opt::FillUsesPass,
    opt::CooperFillDominatorsPass, opt::FillDominanceFrontierPass,
    opt::SSAConstructPass, opt::FillUsesPass, opt::GVNPass, opt::FillUsesPass,
    opt::SimpleDeadCodeEliminationPass, opt::FillPredsPass,
    opt::SSADestructPass, opt::LocalConstAndCopyPropagationPass,
    opt::FillUsesPass, opt::SimpleDeadCodeEliminationPass, opt::FillPredsPass,
    opt::SimplifyCFGPass, opt::LocalConstAndCopyPropagationPass,
    opt::FillUsesPass, opt::SimpleDeadCodeEliminationPass, opt::FillPredsPass,
    opt::SimplifyCFGPass>;
```

## 七、目标代码生成设计

本编译器以 RISC-V 汇编码为目标代码。在中间代码优化之后，我们首先通过 `LinearScanAllocator` 类进行寄存器分配，为应当分配寄存器的虚拟寄存器分配对应的物理寄存器。之后，我们通过 `StackManager` 类分配每个函数的栈空间，包括设定全局寄存器的保存位置、变量的保存位置、函数传参的保存位置等等。

最后，我们在 `Generator` 类中实现了目标代码生成。在生成指令的时候，我们额外设定了 `PeepholeBuffer` 类用于暂时存储生成的指令并在进行窥孔优化。在优化之后，我们再将指令统一输出。

