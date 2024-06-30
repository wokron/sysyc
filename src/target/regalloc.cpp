#include "target/regalloc.h"
#include <algorithm>
#include <set>

namespace target {

void LinearScanAllocator::allocate_registers(ir::Function &func) {
    _register_map.clear();
    _find_global_temps(func);
    _pre_allocate_temps(func);
    _allocate_global_temps(func);
    _allocate_local_temps(func);
}

void LinearScanAllocator::_find_global_temps(ir::Function &func) {
    _global_temps.clear();
    std::unordered_multimap<ir::TempPtr, ir::BlockPtr> temp_block_map;
    std::vector<ir::TempPtr> temps;

    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (inst->to) {
                temps.push_back(inst->to);
                temp_block_map.insert({inst->to, block});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0]);
                temp) {
                temp_block_map.insert({temp, block});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[1]);
                temp) {
                temp_block_map.insert({temp, block});
            }
        }
    }

    for (auto temp : temps) {
        auto range = temp_block_map.equal_range(temp);
        auto blocks_count = std::distance(range.first, range.second);
        if (blocks_count > 1) {
            _global_temps.insert(temp);
        }
    }
}

void LinearScanAllocator::_pre_allocate_temps(ir::Function &func) {
    // allocate a registers for function parameters
    int arg_index = 0;
    for (auto inst : func.start->insts) {
        if (inst->insttype != ir::InstType::IPAR) {
            continue;
        }
        if (arg_index >= 8) {
            break;
        }
        if (_global_temps.find(inst->to) != _global_temps.end()) {
            // use s registers for global temps
            continue;
        }
        if (inst->to->get_type() == ir::Type::S) {
            // f10-f17
            _register_map[inst->to] = 32 + 10 + arg_index;
        } else {
            // x10-x17
            _register_map[inst->to] = 10 + arg_index;
        }
        arg_index++;
    }

    // allocate a registers for call arguments
    arg_index = 0;
    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (inst->insttype != ir::InstType::IARG) {
                arg_index = 0;
                continue;
            }
            if (arg_index >= 8) {
                break;
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0]);
                temp) {
                if (_global_temps.find(temp) != _global_temps.end() ||
                    temp->uses.size() > 1) {
                    continue;
                }
                if (temp->get_type() == ir::Type::S) {
                    // f10-f17
                    _register_map[temp] = 32 + 10 + arg_index;
                } else {
                    // x10-x17
                    _register_map[temp] = 10 + arg_index;
                }
                arg_index++;
            }
        }
    }
}

void LinearScanAllocator::_allocate_global_temps(ir::Function &func) {
    std::vector<TempInterval> intervals;
    std::vector<TempInterval> f_intervals;
    _find_intervals(func, intervals, f_intervals);

    // s registers (x9, x18-x27)
    std::unordered_set<int> reg_set = {9,  18, 19, 20, 21, 22,
                                       23, 24, 25, 26, 27};

    // fs registers (f8-f9, f18-f27)
    std::unordered_set<int> f_reg_set = {32 + 8,  32 + 9,  32 + 18, 32 + 19,
                                         32 + 20, 32 + 21, 32 + 22, 32 + 23,
                                         32 + 24, 32 + 25, 32 + 26, 32 + 27};

    //  allocate s registers
    _allocate_global_temps(func, intervals, reg_set);

    // allocate fs registers
    _allocate_global_temps(func, f_intervals, f_reg_set);
}

void LinearScanAllocator::_allocate_global_temps(
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

void LinearScanAllocator::_allocate_local_temps(ir::Function &func) {
    // TODO: implement
    throw std::logic_error("not implemented");
    // for (auto block = func.start; block; block = block->next) {
    //     // t registers (x5-x7, x28-x31)
    //     std::unordered_set<int> free_regs = {5, 6, 7, 28, 29, 30, 31};
    //     // ft registers (f0-f7, f28-f31)
    //     std::unordered_set<int> f_free_regs = {
    //         32 + 0, 32 + 1, 32 + 2,  32 + 3,  32 + 4,  32 + 5,
    //         32 + 6, 32 + 7, 32 + 28, 32 + 29, 32 + 30, 32 + 31};

    //     for (auto it = block->insts.rbegin(); it != block->insts.rend();
    //     it++) {
    //         auto inst = *it;
    //         if (inst->to) { // def, live range end
    //             auto free_regs_ptr = inst->to->get_type() == ir::Type::S
    //                                      ? &f_free_regs
    //                                      : &free_regs;
    //             free_regs_ptr->insert(_register_map[inst->to]);
    //         }

    //         for (int i = 0; i < 2; i++)
    //             if (auto temp =
    //                     std::dynamic_pointer_cast<ir::Temp>(inst->arg[i]);
    //                 temp) {
    //                 if (_register_map.find(temp) ==
    //                     _register_map.end()) { // last use, live range start
    //                     // allocate register
    //                     auto free_regs_ptr = temp->get_type() == ir::Type::S
    //                                              ? &f_free_regs
    //                                              : &free_regs;
    //                     if (!free_regs_ptr->empty()) {
    //                         auto reg = *(free_regs_ptr->begin());
    //                         free_regs_ptr->erase(reg);
    //                         _register_map[temp] = reg;
    //                     } else {
    //                         // spill
    //                     }
    //                 }
    //             }
    //     }
    // }
}

void LinearScanAllocator::_find_intervals(
    const ir::Function &func, std::vector<TempInterval> &intervals,
    std::vector<TempInterval> &f_intervals) {
    std::unordered_map<ir::InstPtr, int> inst_number;
    _inst_numbering(func, inst_number);

    std::unordered_map<ir::TempPtr, Interval> temp_intervals;
    std::unordered_map<ir::TempPtr, Interval> f_temp_intervals;

    for (auto block : func.rpo) {
        for (auto inst : block->insts) {
            if (inst->to &&
                _global_temps.find(inst->to) != _global_temps.end()) {
                auto temp_intervals_ptr = inst->to->get_type() == ir::Type::S
                                              ? &f_temp_intervals
                                              : &temp_intervals;
                if (temp_intervals_ptr->find(inst->to) ==
                    temp_intervals_ptr->end()) {
                    temp_intervals_ptr->insert(
                        {inst->to, {inst_number[inst], -1}});
                }
            }

            // TODO: update interval end point
            throw std::logic_error("not implemented");
        }
    }

    for (auto [temp, interval] : temp_intervals) {
        intervals.push_back({temp, interval});
    }
    for (auto [temp, interval] : f_temp_intervals) {
        f_intervals.push_back({temp, interval});
    }
}

void LinearScanAllocator::_inst_numbering(
    const ir::Function &func,
    std::unordered_map<ir::InstPtr, int> &inst_number) {
    int number = 0;
    for (auto block : func.rpo) {
        for (auto inst : block->insts) {
            inst_number[inst] = number++;
        }
    }
}

} // namespace target
