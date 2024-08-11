#include "target/regalloc.h"
#include <algorithm>
#include <limits>
#include <set>

namespace target {

void LinearScanAllocator::allocate_registers(const ir::Function &func) {
    _register_map.clear();
    _allocate_temps(func);
}

void LinearScanAllocator::_allocate_temps(const ir::Function &func) {
    std::vector<ir::TempPtr> intervals;
    std::vector<ir::TempPtr> f_intervals;
    std::vector<ir::TempPtr> local_intervals;
    std::vector<ir::TempPtr> f_local_intervals;
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

    // just for debug
    for (auto temp : func.temps_in_func) {
        if (temp->reg == NO_REGISTER) {
            throw std::runtime_error("no register allocated");
        }
        _register_map[temp] = temp->reg;
    }
}

void LinearScanAllocator::_allocate_temps_with_intervals(
    const ir::Function &func, std::vector<ir::TempPtr> &intervals,
    std::unordered_set<int> &reg_set) {
    // sort intervals by start point
    std::sort(intervals.begin(), intervals.end(),
              [](const ir::TempPtr &a, const ir::TempPtr &b) {
                  return a->interval.start < b->interval.start;
              });

    auto asc_end = [](const ir::TempPtr &a, const ir::TempPtr &b) {
        // "a and b are considered equivalent (not unique) if neither compares
        // less than the other", so we need to use <= instead of < to make sure
        // the set can store multiple elements with the same end
        return a->interval.end <= b->interval.end;
    };
    // create active list (ordered set as a double-ended priority queue)
    std::set<ir::TempPtr, decltype(asc_end)> active(asc_end);

    for (auto temp : intervals) {
        auto interval = temp->interval;
        while (active.size() > 0 &&
               (*active.begin())->interval.end <= interval.start) {
            auto front_active = *active.begin();
            active.erase(active.begin());
            reg_set.insert(front_active->reg);
        }

        constexpr auto is_stack = [=](const ir::InstPtr &def_inst) {
            constexpr auto is_indirect_stack = [](const ir::InstPtr &def_inst) {
                if (def_inst->insttype == ir::InstType::IADD) {
                    auto temp_arg0 =
                        std::dynamic_pointer_cast<ir::Temp>(def_inst->arg[0]);
                    if (temp_arg0 == nullptr)
                        return false;
                    auto alloc_inst =
                        std::get<ir::InstDef>(temp_arg0->defs[0]).ins;
                    auto const_arg1 = std::dynamic_pointer_cast<ir::ConstBits>(
                        def_inst->arg[1]);
                    return (alloc_inst->insttype == ir::InstType::IALLOC4 ||
                            alloc_inst->insttype == ir::InstType::IALLOC8) &&
                           const_arg1 != nullptr;
                } else {
                    return false;
                }
            };
            return def_inst->insttype == ir::InstType::IALLOC4 ||
                   def_inst->insttype == ir::InstType::IALLOC8 ||
                   is_indirect_stack(def_inst);
        };

        // if is stack slot, just spill
        if (temp->defs.size() == 1)
            if (auto inst_def = std::get_if<ir::InstDef>(&temp->defs[0]);
                inst_def && is_stack(inst_def->ins)) {
                temp->reg = STACK;
                continue;
            }

        if (reg_set.empty()) {
            if (active.size() > 0 && (*std::prev(active.end()))->interval.end >=
                                         interval.end) { // spill other temp
                auto back_active = *std::prev(active.end());
                active.erase(std::prev(active.end()));

                temp->reg = back_active->reg;
                back_active->reg = SPILL; // in memory
                active.insert(temp);
            } else {               // spill current temp
                temp->reg = SPILL; // in memory
            }
        } else {
            // allocate register
            auto reg = *(reg_set.begin());
            reg_set.erase(reg);
            temp->reg = reg;
            if (!active.insert(temp).second) {
                throw std::runtime_error("Failed to insert into active list");
            }
        }
    }
}

void LinearScanAllocator::_find_intervals(
    const ir::Function &func, std::vector<ir::TempPtr> &intervals,
    std::vector<ir::TempPtr> &f_intervals,
    std::vector<ir::TempPtr> &local_intervals,
    std::vector<ir::TempPtr> &f_local_intervals) {

    std::vector<ir::TempPtr> *intervals_ptrs[2][2] = {
        {&intervals, &f_intervals}, {&local_intervals, &f_local_intervals}};

    for (auto temp : func.temps_in_func) {
        auto is_float = temp->get_type() == ir::Type::S;
        auto is_local = temp->is_local = _is_local(temp);
        auto &intervals_ref = *intervals_ptrs[is_local][is_float];

        intervals_ref.push_back(temp);
    }
}

bool LinearScanAllocator::_is_local(const ir::TempPtr &temp) {
    std::unordered_set<ir::BlockPtr> blocks;
    for (auto def : temp->defs) {
        if (auto inst_def = std::get_if<ir::InstDef>(&def); inst_def) {
            blocks.insert(inst_def->blk);
        } else if (auto phi_def = std::get_if<ir::PhiDef>(&def); phi_def) {
            blocks.insert(phi_def->blk);
        } else {
            throw std::logic_error("unknown def type");
        }
    }
    for (auto use : temp->uses) {
        if (auto inst_use = std::get_if<ir::InstUse>(&use); inst_use) {
            blocks.insert(inst_use->blk);
        } else if (auto phi_use = std::get_if<ir::PhiUse>(&use); phi_use) {
            blocks.insert(phi_use->blk);
        } else if (auto jump_use = std::get_if<ir::JmpUse>(&use); jump_use) {
            blocks.insert(jump_use->blk);
        } else {
            throw std::logic_error("unknown use type");
        }
    }
    return blocks.size() <= 1;
}

} // namespace target
