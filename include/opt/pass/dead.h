#pragma once

#include "opt/pass/base.h"
#include <unordered_set>

namespace opt {

/**
 * @brief Simple dead code elimination pass
 * @note This pass requires `FillUsesPass`
 * @warning This pass will break use-def relationship filled by `FillUsesPass`
 */
class SimpleDeadCodeEliminationPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;
    
  private:
    void _mark_always_alive(ir::Function &func);
    void _mark_all_alive(ir::Function &func);
    bool _remove_unmarked(ir::Function &func);

    static void _insert_if_temp(std::unordered_set<ir::TempPtr> &set, ir::ValuePtr value);

    std::unordered_set<ir::TempPtr> _frontier;

};

} // namespace opt
