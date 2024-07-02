#pragma once

#include "ir/ir.h"
#include "ostream"

namespace target {
class Generator {
public:
    Generator() = default;
    Generator(std::ostream &out) : _out(out) {}

    void generate(const ir::Module &module);

    void generate_data(const ir::Data &data);
    void generate_func(const ir::Function &func);

private:
    std::ostream &_out = std::cout;
};

} // namespace target
