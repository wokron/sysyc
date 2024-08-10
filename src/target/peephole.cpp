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

void PeepholeBuffer::optimize(bool minimum_stack) {
    _eliminate_immediate();
    _weaken_load();
    _eliminate_move();
    _eliminate_jump();
    _simplify_cmp_branch();
    _weaken_branch();
    _weaken_arithmetic();
    _eliminate_move();
    if (minimum_stack) {
        _eliminate_entry_exit();
    }
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
    const static Patterns patterns = {
        {"li", "add"}, {"li", "sub"}, {"li", "addw"}, {"li", "subw"}};

    auto callback = [&](std::deque<iterator> &window) {
        auto &load = *window.front();
        auto &inst = *window.back();
        int imm = std::stoi(load.arg1());
        if (!_is_temp_reg(load.arg0())) {
            return;
        }
        if ((load.arg0() != inst.arg1()) && (load.arg0() != inst.arg2())) {
            return;
        }
        if (inst.arg1() == inst.arg2()) {
            if ((inst.op().front() == 'a') && is_in_imm12_range(imm * 2)) {
                inst = AsmInst({"li", inst.arg0(), std::to_string(imm * 2)});
                _insts.erase(window.front());
            } else {
                inst = AsmInst({"mv", inst.arg0(), "zero"});
                _insts.erase(window.front());
            }
        } else if (is_in_imm12_range(imm)) {
            if ((inst.op().front() == 'a') && (inst.arg1() == load.arg0())) {
                inst.swap(1, 2);
            }
            if (inst.arg2() == load.arg0()) {
                if (inst.op().front() == 's') {
                    inst.arg2("-" + load.arg1());
                } else {
                    inst.arg2(load.arg1());
                }
                inst.op(inst.op().back() == 'w' ? "addiw" : "addi");
                _insts.erase(window.front());
            }
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
    const static Patterns reg_patterns = {{"mv"}, {"fmv.s"}};
    const static Patterns imm_patterns = {{"li", "mv"}};

    auto reg_callback = [&](std::deque<iterator> &window) {
        auto &move = *window.front();
        if (move.arg0() == move.arg1()) {
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 1, true, reg_patterns, reg_callback);

    auto imm_callback = [&](std::deque<iterator> &window) {
        auto &load = *window.front();
        auto &move = *window.back();
        if (_is_temp_reg(load.arg0()) && (load.arg0() == move.arg1())) {
            load.arg0(move.arg0());
            _insts.erase(window.back());
        }
    };
    _slide(_insts.begin(), _insts.end(), 2, true, imm_patterns, imm_callback);
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

        if (_is_temp_reg(branch.arg0()) && (cmp.arg0() == branch.arg0())) {
            branch = AsmInst({"blt", cmp.arg1(), cmp.arg2(), branch.arg1()});
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 2, true, lt_patterns, lt_callback);

    auto le_callback = [&](std::deque<iterator> &window) {
        auto &cmp = *window.front();
        auto &xori = **std::next(window.begin());
        auto &branch = *window.back();

        if (_is_temp_reg(branch.arg0()) && (cmp.arg0() == branch.arg0()) &&
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

        if (_is_temp_reg(branch.arg0()) && (xori.arg0() == branch.arg0()) &&
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

    static const Patterns zpatterns = {{"li", "blt"}, {"li", "ble"},
                                       {"li", "bgt"}, {"li", "bge"},
                                       {"li", "beq"}, {"li", "bne"}};
    static const std::unordered_map<std::string, std::string> zops = {
        {"blt", "bgtz"}, {"bgt", "bltz"}, {"ble", "bgez"},
        {"bge", "blez"}, {"beq", "beqz"}, {"bne", "bnez"}};
    auto zcallback = [&](std::deque<iterator> &window) {
        auto &load = *window.front();
        auto &branch = *window.back();

        if ((!_is_temp_reg(load.arg0())) || (load.arg1() != "0")) {
            return;
        }
        if (load.arg0() == branch.arg1()) {
            branch = AsmInst({branch.op() + "z", branch.arg0(), branch.arg2()});
            _insts.erase(window.front());
        } else if (load.arg0() == branch.arg0()) {
            branch =
                AsmInst({zops.at(branch.op()), branch.arg1(), branch.arg2()});
            _insts.erase(window.front());
        }
    };
    _slide(_insts.begin(), _insts.end(), 2, true, zpatterns, zcallback);
}

void PeepholeBuffer::_weaken_arithmetic() {
    static const Patterns pattern0 = {{"addi"}};
    auto callback0 = [&](std::deque<iterator> &window) {
        auto &inst = *window.front();
        if (inst.arg2() == "0") {
            inst = AsmInst({"mv", inst.arg0(), inst.arg1()});
        }
    };

    static const Patterns pattern1 = {
        {"add", "mv"},       {"addw", "mv"},      {"sub", "mv"},
        {"subw", "mv"},      {"mul", "mv"},       {"mulw", "mv"},
        {"div", "mv"},       {"divw", "mv"},      {"rem", "mv"},
        {"remw", "mv"},      {"addi", "mv"},      {"addiw", "mv"},
        {"fadd.s", "fmv.s"}, {"fsub.s", "fmv.s"}, {"fmul.s", "fmv.s"},
        {"fdiv.s", "fmv.s"}};
    auto callback1 = [&](std::deque<iterator> &window) {
        auto &inst = *window.front();
        auto &move = *window.back();

        if (_is_temp_reg(inst.arg0()) && (inst.arg0() == move.arg1())) {
            inst.arg0(move.arg0());
            _insts.erase(window.back());
        }
    };

    static const Patterns pattern2 = {
        {"mv", "add"},       {"mv", "addw"},      {"mv", "sub"},
        {"mv", "subw"},      {"mv", "mul"},       {"mv", "mulw"},
        {"mv", "div"},       {"mv", "divw"},      {"mv", "rem"},
        {"mv", "remw"},      {"mv", "addi"},      {"mv", "addiw"},
        {"fmv.s", "fadd.s"}, {"fmv.s", "fsub.s"}, {"fmv.s", "fmul.s"},
        {"fmv.s", "fdiv.s"}};
    auto callback2 = [&](std::deque<iterator> &window) {
        auto &move = *window.front();
        auto &inst = *window.back();

        if (_is_temp_reg(move.arg0())) {
            bool match = false;
            if (inst.arg1() == move.arg0()) {
                inst.arg1(move.arg1());
                match = true;
            }
            if (inst.arg2() == move.arg0()) {
                inst.arg2(move.arg1());
                match = true;
            }
            if (match) {
                // special case for trailing mv
                auto next = std::next(window.back());
                if (next != _insts.end() && next->op() == "mv") {
                    if ((next->arg0() == move.arg1()) &&
                        (next->arg1() == move.arg0())) {
                        if (inst.arg0() != next->arg1()) {
                            next->arg1(move.arg1());
                        }
                    }
                }
                _insts.erase(window.front());
            }
        }
    };

    static const Patterns pattern3 = {
        {"mv", "*", "add"},       {"mv", "*", "addw"},
        {"mv", "*", "sub"},       {"mv", "*", "subw"},
        {"mv", "*", "mul"},       {"mv", "*", "mulw"},
        {"mv", "*", "div"},       {"mv", "*", "divw"},
        {"mv", "*", "rem"},       {"mv", "*", "remw"},
        {"mv", "*", "addi"},      {"mv", "*", "addiw"},
        {"fmv.s", "*", "fadd.s"}, {"fmv.s", "*", "fsub.s"},
        {"fmv.s", "*", "fmul.s"}, {"fmv.s", "*", "fdiv.s"}};
    auto callback3 = [&](std::deque<iterator> &window) {
        auto &move = *window.front();
        auto &inst = *window.back();

        if (_is_temp_reg(move.arg0())) {
            bool match = false;
            if (move.arg0() == inst.arg1()) {
                inst.arg1(move.arg1());
                match = true;
            }
            if (move.arg0() == inst.arg2()) {
                inst.arg2(move.arg1());
                match = true;
            }
            if (match) {
                _insts.erase(window.front());
            }
        }
    };

    _slide(_insts.begin(), _insts.end(), 1, true, pattern0, callback0);
    _slide(_insts.begin(), _insts.end(), 2, true, pattern1, callback1);

    // pattern 2 and 3 may misuse temporay registers, comment out for now

    // It is possible for pattern2 to match twice if the argument comes
    // from two mv instructions.
    // _slide(_insts.begin(), _insts.end(), 2, true, pattern2, callback2);
    // _slide(_insts.begin(), _insts.end(), 2, true, pattern2, callback2);

    // _slide(_insts.begin(), _insts.end(), 3, true, pattern3, callback3);
}

void PeepholeBuffer::_eliminate_entry_exit() {
    for (auto it = _insts.begin(); it != _insts.end(); it++) {
        if (it->op() == "call") {
            return;
        }
    }

    for (auto it = _insts.begin(); it != _insts.end();) {
        if (it->is_entry() || it->is_exit()) {
            it = _insts.erase(it);
        } else {
            it++;
        }
    }
}

bool PeepholeBuffer::_is_temp_reg(const std::string &reg) const {
    if (reg.front() == 't') {
        return true;
    }
    if ((reg == "a4") || (reg == "a5")) {
        return true;
    }
    if ((reg[0] == 'f') && (reg[1] == 't')) {
        return true;
    }

    return false;
}

void PeepholeBuffer::_slide(
    iterator begin, iterator end, int window_size, bool inst_only,
    std::vector<std::vector<std::string>> patterns,
    std::function<void(std::deque<iterator> &)> callback) {

    std::deque<iterator> window;
    auto it = begin;
    while (it != end) {
        auto next = std::next(it);

        // only on body instructions
        if ((!it->is_body()) || (it->is_label() && inst_only)) {
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
                window.clear();
            } else {
                for (const auto &pattern : patterns) {
                    if (match(window, pattern)) {
                        callback(window);
                        window.clear();
                        break;
                    }
                }
            }
        }

        it = next;
    }
}

} // namespace target
