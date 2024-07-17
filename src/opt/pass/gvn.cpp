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
        str = _build_inst_string(instdef->ins);
    } else if (auto phidef = std::get_if<ir::PhiDef>(&temp->defs[0])) {
        for (auto [block, value] : phidef->phi->args) {
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
                if (_cache.find(temp) == _cache.end()) {
                    auto hash = _counter++;
                    _cache.insert({temp, hash});
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
    std::string ret;
    ret.append(ir::inst2name[inst->insttype]).append(" ");
    if (inst->arg[0]) {
        ret.append(_build_value_string(inst->arg[0]));
    }
    if (inst->arg[1]) {
        ret.append(", ").append(_build_value_string(inst->arg[1]));
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

bool opt::GVNPass::run_on_function(ir::Function &func) {
    _hasher.reset();
    // dom_rpo is diff from rpo, it's reverse post order of dominator tree
    std::vector<ir::BlockPtr> dom_rpo;
    _get_dom_tree_rpo(func, dom_rpo);

    std::unordered_map<int, ir::TempPtr>
        hash_temp_map; // get temp with the same hash
    std::unordered_map<ir::ValuePtr, ir::ValuePtr>
        value_map; // value for replacement

    for (auto block : dom_rpo) {
        for (auto phi : block->phis) {
            for (auto &[block, value] : phi->args) {
                if (auto it = value_map.find(value); it != value_map.end()) {
                    value = it->second;
                }
            }

            auto hash = _hasher.hash(phi->to);
            if (auto it = hash_temp_map.find(hash); it != hash_temp_map.end()) {
                value_map.insert({phi->to, it->second});
            } else {
                hash_temp_map.insert({hash, phi->to});
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
                if (auto it = hash_temp_map.find(hash);
                    it != hash_temp_map.end()) {
                    value_map.insert({inst->to, it->second});
                } else {
                    hash_temp_map.insert({hash, inst->to});
                }
            }
        }

        if (block->jump.arg != nullptr) {
            if (auto it = value_map.find(block->jump.arg);
                it != value_map.end()) {
                block->jump.arg = it->second;
            }
        }
    }

    return false;
}

void opt::GVNPass::_get_dom_tree_rpo(const ir::Function &func,

                                     std::vector<ir::BlockPtr> &dom_rpo) {
    _dom_tree_traverse(func.start, dom_rpo);
    std::reverse(dom_rpo.begin(), dom_rpo.end());
}

void opt::GVNPass::_dom_tree_traverse(const ir::BlockPtr block,
                                      std::vector<ir::BlockPtr> &dom_po) {
    for (auto child : block->doms) {
        _dom_tree_traverse(child, dom_po);
    }
    dom_po.push_back(block);
}
