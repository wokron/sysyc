#pragma once

#include "ir/ir.h"
#include <unordered_set>

namespace target {

#define STACK -2
#define SPILL -1
#define NO_REGISTER -3

class RegisterAllocator {
public:
    virtual void allocate_registers(const ir::Function &func) = 0;

    std::unordered_map<ir::TempPtr, int> get_register_map() const {
        return _register_map;
    }

protected:
    std::unordered_map<ir::TempPtr, int> _register_map;
};

class LinearScanAllocator : public RegisterAllocator {
public:
    void allocate_registers(const ir::Function &func) override;

private:
    void _allocate_temps(const ir::Function &func);

    void _allocate_temps_with_intervals(const ir::Function &func,
                                        std::vector<ir::TempPtr> &intervals,
                                        std::unordered_set<int> &reg_set);

    void _find_intervals(const ir::Function &func,
                         std::vector<ir::TempPtr> &intervals,
                         std::vector<ir::TempPtr> &f_intervals,
                         std::vector<ir::TempPtr> &local_intervals,
                         std::vector<ir::TempPtr> &f_local_intervals);

    static bool _is_local(const ir::TempPtr &temp);

    std::unordered_set<ir::TempPtr> _global_temps;
};

} // namespace target
