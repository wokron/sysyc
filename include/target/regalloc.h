#pragma once

#include "ir/ir.h"
#include <unordered_set>

namespace target {

class RegisterAllocator {
public:
    virtual void allocate_registers(ir::Function &func) = 0;

    std::unordered_map<ir::TempPtr, int> get_register_map() const {
        return _register_map;
    }

protected:
    std::unordered_map<ir::TempPtr, int> _register_map;
};

class LinearScanAllocator : public RegisterAllocator {
public:
    void allocate_registers(ir::Function &func) override;

private:
    using Interval = std::pair<int, int>;
    using TempInterval = std::pair<ir::TempPtr, Interval>;

    void _find_global_temps(ir::Function &func);

    void _pre_allocate_temps(ir::Function &func);

    void _allocate_local_temps(ir::Function &func);

    void _allocate_global_temps(ir::Function &func);

    void _allocate_global_temps(ir::Function &func,
                                std::vector<TempInterval> &intervals,
                                std::unordered_set<int> &reg_set);

    void _find_intervals(const ir::Function &func,
                         std::vector<TempInterval> &intervals,
                         std::vector<TempInterval> &f_intervals);

    void _inst_numbering(const ir::Function &func,
                         std::unordered_map<ir::InstPtr, int> &inst_number);

    std::unordered_set<ir::TempPtr> _global_temps;
};

} // namespace target
