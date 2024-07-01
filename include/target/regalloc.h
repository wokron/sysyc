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
    struct Interval {
        int start;
        int end;
    };
    using TempInterval = std::pair<ir::TempPtr, Interval>;

    void _allocate_temps(ir::Function &func);

    void _allocate_temps_with_intervals(ir::Function &func,
                                std::vector<TempInterval> &intervals,
                                std::unordered_set<int> &reg_set);

    void _find_intervals(const ir::Function &func,
                         std::vector<TempInterval> &intervals,
                         std::vector<TempInterval> &f_intervals,
                         std::vector<TempInterval> &local_intervals,
                         std::vector<TempInterval> &f_local_intervals);

    void _find_intervals_in_block(
        const ir::Block &block, std::unordered_map<ir::TempPtr, int> &first_def,
        std::unordered_map<ir::TempPtr, int> &last_use,
        std::unordered_set<ir::TempPtr> &temps_in_block, int &number);

    std::unordered_set<ir::TempPtr> _global_temps;
};

} // namespace target
