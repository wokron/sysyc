#include "opt/pass/gvn.h"
#include <algorithm>
#include <sstream>

int opt::HashHelper::hash(ir::TempPtr temp) {
    if (auto it = _cache.find(temp); it != _cache.end()) {
        return it->second;
    }

    if (temp->defs.size() != 1) {
        throw std::logic_error("single def is required");
    }

    std::string str;
    if (auto instdef = std::get_if<ir::InstDef>(&temp->defs[0])) {
        if (_has_side_effect(instdef->ins->insttype)) {
            auto hash = _counter++;
            _cache.insert({temp, hash});
            return hash;
        } else if (instdef->ins->insttype ==
                   ir::InstType::ICOPY) { // special treate copy
            if (auto temp =
                    std::dynamic_pointer_cast<ir::Temp>(instdef->ins->arg[0])) {
                return _cache.at(temp);
            }
        }
        str = _build_inst_string(instdef->ins);
    } else if (auto phidef = std::get_if<ir::PhiDef>(&temp->defs[0])) {
        for (auto [block, value] : phidef->phi->args) {
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
                if (_cache.find(temp) == _cache.end()) {
                    auto hash = _counter++;
                    _cache.insert({phidef->phi->to, hash});
                    return hash;
                }
            }
        }
        str = _build_inst_string(phidef->phi);
    } else {
        throw std::logic_error("invalid def type");
    }

    int hash;
    if (auto it = _str_hash_map.find(str); it != _str_hash_map.end()) {
        hash = it->second;
    } else {
        hash = _counter++;
        _str_hash_map.insert({str, hash});
    }

    _cache.insert({temp, hash});
    return hash;
}

void opt::HashHelper::reset() {
    _cache.clear();
    _str_hash_map.clear();
    _counter = 0;
}

std::string opt::HashHelper::_build_inst_string(ir::InstPtr inst) {
    // just like ir::Inst::emit, but use hash(temp) to replace temp argument
    auto arg0 = inst->arg[0], arg1 = inst->arg[1];

    // satisfy the commutative law
    if (inst->insttype == ir::InstType::IADD ||
        inst->insttype == ir::InstType::IMUL) {
        // we need to make sure that `b + a` has the same hash with `a + b`
        if (arg0.get() > arg1.get()) {
            arg0.swap(arg1);
        }
    }

    std::string ret;
    ret.append(ir::inst2name[inst->insttype]).append(" ");
    if (arg0) {
        ret.append(_build_value_string(arg0));
    }
    if (arg1) {
        ret.append(", ").append(_build_value_string(arg1));
    }

    return ret;
}

std::string opt::HashHelper::_build_inst_string(ir::PhiPtr phi) {
    // just like ir::Phi::emit, but use hash(temp) to replace temp argument
    std::string ret;
    ret.append("phi").append(" ");
    bool first = true;
    for (auto &[block, value] : phi->args) {
        if (first) {
            first = false;
        } else {
            ret.append(", ");
        }
        ret.append("@").append(block->get_name()).append(" ");
        if (value == nullptr) {
            ret.append("0");
        } else {
            ret.append(_build_value_string(value));
        }
    }
    return ret;
}

std::string opt::HashHelper::_build_value_string(ir::ValuePtr value) {
    if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
        std::string ret;
        ret.append("%").append(
            std::to_string(_cache.at(temp))); // assert hash(temp) exist
        return ret;
    }
    std::ostringstream out;
    value->emit(out);
    return out.str();
}

bool opt::HashHelper::_has_side_effect(ir::InstType insttype) {
    switch (insttype) {
    case ir::InstType::IALLOC4:
    case ir::InstType::IALLOC8:
    case ir::InstType::ILOADS:
    case ir::InstType::ILOADL:
    case ir::InstType::ILOADW:
    case ir::InstType::IPAR:
    case ir::InstType::ICALL:
        return true;
    default:
        return false;
    }
}

bool opt::GVNPass::run_on_function(ir::Function &func) {
    _hasher.reset();
    _dom_tree_traverse(func.start, {}, {});

    return false;
}

void opt::GVNPass::_dom_tree_traverse(
    const ir::BlockPtr block,
    std::unordered_map<int, ir::ValuePtr> hash_temp_map,
    std::unordered_map<ir::ValuePtr, ir::ValuePtr> value_map) {

    for (auto phi : block->phis) {
        std::unordered_set<ir::ValuePtr> arg_set;
        for (auto &[block, value] : phi->args) {
            if (auto it = value_map.find(value); it != value_map.end()) {
                value = it->second;
            }
            arg_set.insert(value);
        }

        auto hash = _hasher.hash(phi->to);
        if (auto it = hash_temp_map.find(hash); it != hash_temp_map.end()) {
            value_map.insert({phi->to, it->second});
        } else {
            if (arg_set.size() ==
                1) { // if has only one value, just like a copy
                auto fold_result = *arg_set.begin();
                value_map.insert({phi->to, fold_result});
                hash_temp_map.insert({hash, fold_result});
            } else {
                hash_temp_map.insert({hash, phi->to});
            }
        }
    }

    for (auto inst : block->insts) {
        for (int i = 0; i < 2; i++)
            if (inst->arg[i] != nullptr) {
                if (auto it = value_map.find(inst->arg[i]);
                    it != value_map.end()) {
                    inst->arg[i] = it->second;
                }
            }

        if (inst->to != nullptr) {
            auto hash = _hasher.hash(inst->to);
            if (auto it = hash_temp_map.find(hash); it != hash_temp_map.end()) {
                value_map.insert({inst->to, it->second});
            } else {
                if (auto fold_result = _fold_if_can(*inst)) { // fold
                    value_map.insert({inst->to, fold_result});
                    hash_temp_map.insert({hash, fold_result});
                } else {
                    hash_temp_map.insert({hash, inst->to});
                }
            }
        }
    }

    if (block->jump.arg != nullptr) {
        if (auto it = value_map.find(block->jump.arg); it != value_map.end()) {
            block->jump.arg = it->second;
        }
    }

    std::vector<ir::BlockPtr> succs;
    switch (block->jump.type) {
    case ir::Jump::JNZ:
        succs.push_back(block->jump.blk[1]);
    case ir::Jump::JMP:
        succs.push_back(block->jump.blk[0]);
        break;
    default:
        break;
    }

    for (auto succ : succs) {
        for (auto phi : succ->phis) {
            for (auto &[from_block, value] : phi->args) {
                if (from_block == block) {
                    if (auto it = value_map.find(value);
                        it != value_map.end()) {
                        value = it->second;
                    }
                }
            }
        }
    }

    std::sort(block->doms.begin(), block->doms.end(),
              [](const ir::BlockPtr &a, const ir::BlockPtr &b) {
                  return a->rpo_id < b->rpo_id;
              });

    for (auto child : block->doms) {
        _dom_tree_traverse(child, hash_temp_map, value_map);
    }
}

ir::ValuePtr opt::GVNPass::_fold_if_can(const ir::Inst &inst) {

    switch (inst.insttype) {
    case ir::InstType::ICOPY:
        return inst.arg[0];
    case ir::InstType::IADD:
        return _folder.fold_add(inst.arg[0], inst.arg[1]);
    case ir::InstType::ISUB:
        return _folder.fold_sub(inst.arg[0], inst.arg[1]);
    case ir::InstType::INEG:
        return _folder.fold_neg(inst.arg[0]);
    case ir::InstType::IMUL:
        return _folder.fold_mul(inst.arg[0], inst.arg[1]);
    case ir::InstType::IDIV:
        return _folder.fold_div(inst.arg[0], inst.arg[1]);
    case ir::InstType::IREM:
        return _folder.fold_rem(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICEQW:
    case ir::InstType::ICEQS:
        return _folder.fold_eq(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICNEW:
    case ir::InstType::ICNES:
        return _folder.fold_ne(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSLEW:
    case ir::InstType::ICLES:
        return _folder.fold_le(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSLTW:
    case ir::InstType::ICLTS:
        return _folder.fold_lt(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSGEW:
    case ir::InstType::ICGES:
        return _folder.fold_ge(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSGTW:
    case ir::InstType::ICGTS:
        return _folder.fold_gt(inst.arg[0], inst.arg[1]);
    case ir::InstType::IEXTSW:
        return _folder.fold_extsw(inst.arg[0]);
    default:
        return nullptr;
    }
}
