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

        if (reg_set.empty()) {
            auto back_active = (*--active.end());
            if (back_active.end >= interval.end) { // spill other temp
                active.erase(--active.end());
                _register_map[back_active.temp] = -1; // in memory

                auto reg = back_active.reg;
                active.insert({temp, reg, interval.end});
            } else {                      // spill current temp
                _register_map[temp] = -1; // in memory
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

    std::unordered_map<ir::TempPtr, Interval> intervals_map;
    std::unordered_map<ir::TempPtr, Interval> f_intervals_map;
    std::unordered_map<ir::TempPtr, Interval> local_intervals_map;
    std::unordered_map<ir::TempPtr, Interval> f_local_intervals_map;

    std::unordered_map<ir::TempPtr, Interval> *intervals_map_ptrs[2][2] = {
        {&intervals_map, &f_intervals_map},
        {&local_intervals_map, &f_local_intervals_map}};

    int number = 0;
    for (auto block : func.rpo) {
        std::unordered_set<ir::TempPtr> temps_in_block;
        std::unordered_map<ir::TempPtr, int> first_def;
        std::unordered_map<ir::TempPtr, int> last_use;
        int first_number = number;
        _find_intervals_in_block(*block, first_def, last_use, temps_in_block,
                                 number);
        int last_number = number - 1;

        for (auto temp : temps_in_block) {
            auto is_float = temp->get_type() == ir::Type::S;
            auto is_local = block->live_in.find(temp) == block->live_in.end() &&
                            block->live_out.find(temp) == block->live_out.end();
            auto &intervals_map_ref = *intervals_map_ptrs[is_float][is_local];

            if (intervals_map_ref.find(temp) == intervals_map_ref.end()) {
                intervals_map_ref[temp] = {std::numeric_limits<int>::max(), -1};
            }
            if (block->live_in.find(temp) ==
                block->live_in.end()) { // if temp is not in the live_in set
                // live interval extended to the first def
                intervals_map_ref[temp].start =
                    std::min(intervals_map_ref[temp].start, first_def[temp]);
            } else {
                // first number of block is in the live interval
                intervals_map_ref[temp].start =
                    std::min(intervals_map_ref[temp].start, first_number);
            }
            if (block->live_out.find(temp) ==
                block->live_out.end()) { // if temp is not in the live_out set
                // live interval extended to the last use
                intervals_map_ref[temp].end =
                    std::max(intervals_map_ref[temp].end, last_use[temp]);
            } else {
                // last number of block is in the live interval
                intervals_map_ref[temp].end =
                    std::max(intervals_map_ref[temp].end, last_number);
            }
        }
    }

    for (auto [temp, interval] : intervals_map) {
        intervals.push_back({temp, interval});
    }
    for (auto [temp, interval] : f_intervals_map) {
        f_intervals.push_back({temp, interval});
    }
    for (auto [temp, interval] : local_intervals_map) {
        local_intervals.push_back({temp, interval});
    }
    for (auto [temp, interval] : f_local_intervals_map) {
        f_local_intervals.push_back({temp, interval});
    }
}

void LinearScanAllocator::_find_intervals_in_block(
    const ir::Block &block, std::unordered_map<ir::TempPtr, int> &first_def,
    std::unordered_map<ir::TempPtr, int> &last_use,
    std::unordered_set<ir::TempPtr> &temps_in_block, int &number) {
    // insts
    for (auto inst : block.insts) {
        for (int i = 0; i < 2; i++)
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[i]);
                temp) {
                temps_in_block.insert(temp);
                last_use[temp] = number;
            }

        if (inst->to) {
            auto temp = inst->to;
            temps_in_block.insert(temp);
            if (first_def.find(temp) == first_def.end()) {
                first_def[temp] = number;
            }
        }
        number++;
    }
    // jump
    if (block.jump.type == ir::Jump::RET || block.jump.type == ir::Jump::JNZ) {
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block.jump.arg);
            temp) {
            temps_in_block.insert(temp);
            last_use[temp] = number;
        }
    }
    number++;
}

} // namespace target
