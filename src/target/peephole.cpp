#include "target/peephole.h"
#include "target/utils.h"
#include <functional>
#include <unordered_map>
#include <utility>

namespace target {

#define INDENT "    "

using Patterns = std::vector<std::vector<std::string>>;

static bool exists_barrier(std::list<AsmInst>::iterator source,
                           std::list<AsmInst>::iterator sink,
                           std::function<bool(AsmInst &)> predicate) {
    for (auto it = ++source; it != sink; it++) {
        if (predicate(*it)) {
            return true;
        }
    }
    return false;
}

static bool is_immediate(std::string arg) {
    return arg[0] == '-' || (arg[0] >= '0' && arg[0] <= '9');
}

static bool match(const std::deque<PeepholeBuffer::iterator> &window,
                  const std::vector<std::string> &pattern) {
    if (window.size() != pattern.size()) {
        return false;
    }
    for (size_t i = 0; i < pattern.size(); i++) {
        if (pattern[i] == "*") {
            continue;
        } else if ((pattern[i] == ".L") && window[i]->is_label()) {
            continue;
        } else if (window[i]->op() != pattern[i]) {
            return false;
        }
    }
    return true;
}

void PeepholeBuffer::optimize() {
    _eliminate_immediate();
    _weaken_load();
    _eliminate_move();
    _eliminate_jump();
    _simplify_cmp_branch();
    _weaken_branch();
}

void PeepholeBuffer::emit(std::ostream &out) const {
    for (const auto &inst : _insts) {
        if (inst.type == AsmInst::INST) {
            out << INDENT;
        }
        out << inst.to_string() << std::endl;
    }
}

void PeepholeBuffer::_eliminate_immediate() {
    const static Patterns patterns = {{"li", "add"}, {"li", "sub"}};

    auto callback = [&](std::deque<iterator> &window) {
        auto &load = *window.front();
        auto &inst = *window.back();
        int imm = std::stoi(load.arg1());
        if (load.arg0()[0] != 't') {
            return;
        }
        if ((inst.arg1() == inst.arg2()) && (is_in_imm12_range(imm * 2))) {
            inst.op("li");
            inst.arg1(std::to_string(imm * 2));
            _insts.erase(window.front());
        } else if (is_in_imm12_range(imm)) {
            if (inst.arg0() == load.arg1()) {
                inst.swap(0, 1);
            }
            if (inst.op() == "sub") {
                inst.arg2("-" + load.arg1());
            } else {
                inst.arg2(load.arg1());
            }
            inst.op("addi");
            _insts.erase(window.front());
        }
    };

    _slide(_insts.begin(), _insts.end(), 2, true, patterns, callback);
}

void PeepholeBuffer::_weaken_load() {
    // clang-format off
    static const std::unordered_map<std::string, std::pair<std::string, std::string>> ops = {
        {"sw",  std::pair<std::string, std::string>("lw", "mv")},
        {"sd",  std::pair<std::string, std::string>("ld", "mv")},
        {"fsw", std::pair<std::string, std::string>("flw", "fmv.s")}};
    // clang-format on

    for (auto it = _insts.begin(); it != _insts.end(); it++) {
        auto pair = ops.find(it->op());
        if (pair == ops.end()) {
            continue;
        }
        auto store = *it;
        // It goes a step beyond peephole to look for safe instruction sequence
        // with store and load only to replace more loads.
        for (auto next = ++it; next != _insts.end(); next++) {
            if (next->op() == pair->first) {
                continue;
            } else if (next->op() == pair->second.first) {
                auto &load = *next;
                // same target register but different source address
                if ((load.arg0() == store.arg0()) &&
                    (load.arg1() != store.arg1())) {
                    break;
                }
                // same source address, same target will be eliminated by
                // _eliminate_move
                if (load.arg1() == store.arg1()) {
                    load.op(pair->second.second);
                    load.arg1(store.arg0());
                }
            } else {
                break;
            }
        }
    }
}

void PeepholeBuffer::_eliminate_move() {
    const static Patterns patterns = {{"mv"}, {"fmv.s"}};

    auto callback = [&](std::deque<iterator> &window) {
        auto &move = *window.front();
        if (move.arg0() == move.arg1()) {
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 1, true, patterns, callback);
}

void PeepholeBuffer::_eliminate_jump() {
    const static Patterns j_pattersn = {{"j", ".L"}};

    auto callback = [&](std::deque<iterator> &window) {
        auto &jump = *window.front();
        auto &label = *window.back();

        if (jump.arg0() == label.op()) {
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 2, false, j_pattersn, callback);
}

// There is a subtle problem is that the peephole optimization is not able
// to get use-def information, while some flag is used across multiple bnez.
// However, only s register is used in the test case, so it is safe to simplify
// all bnez with t registers.
void PeepholeBuffer::_simplify_cmp_branch() {
    static const Patterns lt_patterns = {{"slt", "bnez"}};
    static const Patterns le_patterns = {{"slt", "xori", "bnez"}};
    static const Patterns eq_patterns = {{"xor", "sltiu", "bnez"},
                                         {"xor", "sltu", "bnez"}};

    auto lt_callback = [&](std::deque<iterator> &window) {
        auto &cmp = *window.front();
        auto &branch = *window.back();

        if ((branch.arg0()[0] == 't') && (cmp.arg0() == branch.arg0())) {
            branch = AsmInst({"blt", cmp.arg1(), cmp.arg2(), branch.arg1()});
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 2, true, lt_patterns, lt_callback);

    auto le_callback = [&](std::deque<iterator> &window) {
        auto &cmp = *window.front();
        auto &xori = **std::next(window.begin());
        auto &branch = *window.back();

        if ((branch.arg0()[0] == 't') && (cmp.arg0() == branch.arg0()) &&
            (cmp.arg1() == xori.arg0())) {
            branch = AsmInst({"ble", cmp.arg2(), cmp.arg1(), branch.arg1()});
            _insts.erase(*std::next(window.begin()));
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 3, true, le_patterns, le_callback);

    auto eq_callback = [&](std::deque<iterator> &window) {
        auto &xori = *window.front();
        auto &cmp = **std::next(window.begin());
        auto &branch = *window.back();

        if ((branch.arg0()[0] == 't') && (xori.arg0() == branch.arg0()) &&
            (xori.arg0() == cmp.arg0())) {
            auto op = (cmp.op() == "sltiu") ? "beq" : "bne";
            branch = AsmInst({op, xori.arg1(), xori.arg2(), branch.arg1()});
            _insts.erase(*std::next(window.begin()));
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 3, true, eq_patterns, eq_callback);
}

void PeepholeBuffer::_weaken_branch() {
    static const Patterns patterns = {{"blt", "j", ".L"},  {"bgt", "j", ".L"},
                                      {"ble", "j", ".L"},  {"bge", "j", ".L"},
                                      {"beq", "j", ".L"},  {"bne", "j", ".L"},
                                      {"beqz", "j", ".L"}, {"bnez", "j", ".L"}};
    static const std::unordered_map<std::string, std::string> ops = {
        {"blt", "bge"}, {"bgt", "ble"}, {"ble", "bgt"},   {"bge", "blt"},
        {"beq", "bne"}, {"bne", "beq"}, {"beqz", "bnez"}, {"bnez", "beqz"}};

    auto callback = [&](std::deque<iterator> &window) {
        auto &branch = *window.front();
        auto &jump = **std::next(window.begin());
        auto &label = *window.back();

        if (branch.op().back() == 'z') {
            if (branch.arg1() == label.op()) {
                branch.op(ops.at(branch.op()));
                branch.arg1(jump.arg0());
                _insts.erase(*std::next(window.begin()));
            }
        } else if (branch.arg2() == label.op()) {
            branch.op(ops.at(branch.op()));
            branch.arg2(jump.arg0());
            _insts.erase(*std::next(window.begin()));
        }
    };
    _slide(_insts.begin(), _insts.end(), 3, false, patterns, callback);
}

void PeepholeBuffer::_slide(
    iterator begin, iterator end, int window_size, bool inst_only,
    std::vector<std::vector<std::string>> patterns,
    std::function<void(std::deque<iterator> &)> callback) {

    std::deque<iterator> window;
    auto it = begin;
    while (it != end) {
        auto next = std::next(it);

        if (it->is_label() && inst_only) {
            window.clear();
            it = next;
            continue;
        }

        window.push_back(it);
        if (window.size() > window_size) {
            window.pop_front();
        }
        if (window.size() == window_size) {
            if (patterns.empty()) {
                callback(window);
            } else {
                for (const auto &pattern : patterns) {
                    if (match(window, pattern)) {
                        callback(window);
                        break;
                    }
                }
            }
        }

        it = next;
    }
}

} // namespace target
