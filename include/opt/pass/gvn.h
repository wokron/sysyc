#pragma once

#include "opt/pass/base.h"

namespace opt {

class HashHelper {
public:
    int hash(ir::TempPtr temp);
    void reset();

private:
    std::string _build_inst_string(ir::InstPtr inst);
    std::string _build_inst_string(ir::PhiPtr phi);
    std::string _build_value_string(ir::ValuePtr value);

    std::unordered_map<ir::TempPtr, int> _cache;
    std::unordered_map<std::string, int> _str_hash_map;
    int _counter = 0;
};

class GVNPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;

  private:
    void _get_dom_tree_rpo(const ir::Function &func, std::vector<ir::BlockPtr> &dom_rpo);
    void _dom_tree_traverse(const ir::BlockPtr block, std::vector<ir::BlockPtr> &dom_po);

    HashHelper _hasher;
};

} // namespace opt
