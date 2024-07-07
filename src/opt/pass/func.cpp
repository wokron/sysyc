#include "opt/pass/func.h"

namespace opt {

bool FillLeafPass::run_on_module(ir::Module &module) {
    for (auto &func : module.functions) {
        func->is_leaf = _is_leaf_function(*func);
    }

    return false;
}
bool FillLeafPass::_is_leaf_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (inst->insttype == ir::InstType::ICALL) {
                return false;
            }
        }
    }
    return true;
}

} // namespace opt
