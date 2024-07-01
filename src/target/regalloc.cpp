#include "target/regalloc.h"
#include <algorithm>
#include <limits>
#include <set>

namespace target {

void LinearScanAllocator::allocate_registers(ir::Function &func) {
    _register_map.clear();
    _allocate_temps(func);
}

void LinearScanAllocator::_allocate_temps(ir::Function &func) {
    std::vector<TempInterval> intervals;
    std::vector<TempInterval> f_intervals;
    std::vector<TempInterval> local_intervals;
    std::vector<TempInterval> f_local_intervals;
    _find_intervals(func, intervals, f_intervals, local_intervals,
                    f_local_intervals);

    // s registers (x9, x18-x27)
    std::unordered_set<int> s_reg_set = {9,  18, 19, 20, 21, 22,
                                         23, 24, 25, 26, 27};
    // allocate s registers
    _allocate_temps_with_intervals(func, intervals, s_reg_set);

    // fs registers (f8-f9, f18-f27)
    std::unordered_set<int> fs_reg_set = {32 + 8,  32 + 9,  32 + 18, 32 + 19,
                                          32 + 20, 32 + 21, 32 + 22, 32 + 23,
                                          32 + 24, 32 + 25, 32 + 26, 32 + 27};
    // allocate fs registers
    _allocate_temps_with_intervals(func, f_intervals, fs_reg_set);

    // t registers (x5-x7, x28-x31)
    std::unordered_set<int> t_reg_set = {5, 6, 7, 28, 29, 30, 31};
    // allocate t registers
    _allocate_temps_with_intervals(func, local_intervals, t_reg_set);

    // ft registers (f0-f7, f28-f31)
    std::unordered_set<int> ft_reg_set = {32 + 0,  32 + 1,  32 + 2,  32 + 3,
                                          32 + 4,  32 + 5,  32 + 6,  32 + 7,
                                          32 + 28, 32 + 29, 32 + 30, 32 + 31};
    // allocate ft registers
    _allocate_temps_with_intervals(func, f_local_intervals, ft_reg_set);
}

void LinearScanAllocator::_allocate_temps_with_intervals(
    ir::Function &func, std::vector<TempInterval> &intervals,
    std::unordered_set<int> &reg_set) {
    // sort intervals by start point
    std::sort(intervals.begin(), intervals.end(),
              [](const TempInterval &a, const TempInterval &b) {
                  auto a_interval = a.second;
                  auto b_interval = b.second;
                  return a_interval.start < b_interval.start;
              });

    struct ActiveInfo {
        ir::TempPtr temp;
        int reg;
        int end;
    };
    auto asc_end = [](const ActiveInfo &a, const ActiveInfo &b) {
        return a.end < b.end;
    };
    // create active list (ordered set as a double-ended priority queue)
    std::set<ActiveInfo, decltype(asc_end)> active(asc_end);

    for (auto [temp, interval] : intervals) {
        while (active.size() > 0 && (*active.begin()).end <= interval.start) {
            auto front_active = *active.begin();
            active.erase(active.begin());

            _register_map[front_active.temp] = front_active.reg;
            reg_set.insert(front_active.reg);
        }

        // if is stack slot, just spill
        if (temp->defs.size() == 1)
            if (auto inst_def = std::get_if<ir::InstDef>(&temp->defs[0]);
                inst_def &&
                (inst_def->ins->insttype == ir::InstType::IALLOC4 ||
                 inst_def->ins->insttype == ir::InstType::IALLOC8)) {
                _register_map[temp] = STACK;
                continue;
            }

        if (reg_set.empty()) {
            auto back_active = (*--active.end());
            if (back_active.end >= interval.end) { // spill other temp
                active.erase(--active.end());
                _register_map[back_active.temp] = SPILL; // in memory

                auto reg = back_active.reg;
                active.insert({temp, reg, interval.end});
            } else {                         // spill current temp
                _register_map[temp] = SPILL; // in memory
            }
        } else {
            // allocate register
            auto reg = *(reg_set.begin());
            reg_set.erase(reg);
            _register_map[temp] = reg;
            active.insert({temp, reg, interval.end});
        }
    }
}

void LinearScanAllocator::_find_intervals(
    const ir::Function &func, std::vector<TempInterval> &intervals,
    std::vector<TempInterval> &f_intervals,
    std::vector<TempInterval> &local_intervals,
    std::vector<TempInterval> &f_local_intervals) {

    std::vector<TempInterval> *intervals_ptrs[2][2] = {
        {&intervals, &f_intervals}, {&local_intervals, &f_local_intervals}};

    for (auto block : func.rpo) {
        for (auto temp : block->temps_in_block) {
            auto is_float = temp->get_type() == ir::Type::S;
            auto is_local = temp->is_local;
            auto &intervals_ref = *intervals_ptrs[is_local][is_float];

            intervals_ref.push_back(
                {temp, {temp->interval.start, temp->interval.end}});
        }
    }
}

} // namespace target
