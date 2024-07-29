#include "target/peephole.h"

namespace target {

#define INDENT "    "

void PeepholeBuffer::optimize() {
    // nothing
}

void PeepholeBuffer::emit(std::ostream &out) const {
    for (const auto &inst : _insts) {
        if (inst.type == AsmInst::INST) {
            out << INDENT;
        }
        out << inst.to_string() << std::endl;
    }
}

} // namespace target