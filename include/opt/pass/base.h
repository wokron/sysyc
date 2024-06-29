// This file defines the base classes for optimization passes.

#pragma once

#include "ir/ir.h"
#include <memory>

namespace opt {

/**
 * @brief Base class for all optimization passes.
 * @param module The module to optimize.
 */
class Pass {
public:
    virtual ~Pass() = default;
    virtual bool run(ir::Module &module) = 0;
};

/**
 * @brief A pass that applies a function to each function in the module.
 * @note This is a base class for all function passes.
 */
class ModulePass : public Pass {
public:
    bool run(ir::Module &module) override { return run_on_module(module); }

    /**
     * @brief Apply the pass to a module.
     * @param module The module to apply the pass to.
     * @return True if the module is changed, false otherwise.
     */
    virtual bool run_on_module(ir::Module &module) = 0;
};

/**
 * @brief A pass that applies a function to each basic block in the function.
 * @note This is a base class for all basic block passes.
 */
class FunctionPass : public ModulePass {
public:
    bool run_on_module(ir::Module &module) override {
        bool changed = false;
        for (auto &func : module.functions) {
            changed |= run_on_function(*func);
        }
        return changed;
    }

    /**
     * @brief Apply the pass to a function.
     * @param func The function to apply the pass to.
     * @return True if the function is changed, false otherwise.
     */
    virtual bool run_on_function(ir::Function &func) = 0;
};

/**
 * @brief A pass that applies a function to each basic block in the function.
 * @note This is a base class for all basic block passes.
 */
class BasicBlockPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override {
        bool changed = false;
        for (auto block = func.start; block; block = block->next) {
            changed |= run_on_basic_block(*block);
        }
        return changed;
    }

    /**
     * @brief Apply the pass to a basic block.
     * @param block The basic block to apply the pass to.
     * @return True if the basic block is changed, false otherwise.
     */
    virtual bool run_on_basic_block(ir::Block &block) = 0;
};

/**
 * @brief PassPipeline is a pass that applies a sequence of passes in order.
 * It is a variadic template class, so you can pass any number of passes to it.
 * @tparam Ps The passes to apply in order.
 * @note All passes must be derived from Pass.
 * @note It is recommended to use alias for PassPipeline, as it can be quite
 * long.
 */
template <typename... Ps> class PassPipeline : public Pass {
public:
    PassPipeline() {
        // template magic to create all passes
        _passes = {new Ps()...};
    }

    ~PassPipeline() {
        for (auto &pass : _passes) {
            delete pass;
        }
    }

    /**
     * @brief Apply all passes in sequence.
     * @param module The module to optimize.
     * @return True if the module is changed, false otherwise.
     */
    bool run(ir::Module &module) override {
        bool changed = false;
        // just apply all passes in sequence
        for (auto &pass : _passes) {
            changed |= pass->run(module);
        }
        return changed;
    }

private:
    // I failed to use unique_ptr here, so I use raw pointers instead
    std::vector<Pass *> _passes;
};

} // namespace opt
