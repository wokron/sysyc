#include <deque>
#include <functional>
#include <initializer_list>
#include <list>
#include <ostream>
#include <string>
#include <vector>

namespace target {

struct AsmInst {
    enum { INST, LABEL } type;
    enum { ENTRY, BODY, EXIT } region;
    std::vector<std::string> args;

    AsmInst(std::initializer_list<std::string> args)
        : type(INST), region(BODY), args(args) {}
    AsmInst(std::string label) : type(LABEL), region(BODY) {
        args.push_back(label);
    }

    std::string to_string() const {
        if (args.empty()) {
            return "nop";
        }

        std::string str = args[0];
        if (type == INST) {
            for (size_t i = 1; i < args.size(); i++) {
                str += (i == 1 ? " " : ", ") + args[i];
            }
        } else {
            str += ":";
        }
        return str;
    }

    bool is_inst() const { return type == AsmInst::INST; }
    bool is_label() const { return type == AsmInst::LABEL; }

    bool is_entry() const { return region == ENTRY; }
    void set_entry() { region = ENTRY; }
    bool is_body() const { return region == BODY; }
    void set_body() { region = BODY; }
    bool is_exit() const { return region == EXIT; }
    void set_exit() { region = EXIT; }

    const std::string &op() const { return args[0]; }
    void op(const std::string &op) { args[0] = op; }
    const std::string &arg0() const { return args[1]; }
    void arg0(const std::string &arg) { args[1] = arg; }
    const std::string &arg1() const { return args[2]; }
    void arg1(const std::string &arg) { args[2] = arg; }
    const std::string &arg2() const { return args[3]; }
    void arg2(const std::string &arg) { args[3] = arg; }

    void swap(int i, int j) { std::swap(args[i + 1], args[j + 1]); }
};

class PeepholeBuffer {
  public:
    using iterator = std::list<AsmInst>::iterator;

    AsmInst &append(std::string op, std::string arg0) {
        return _insts.emplace_back(
            std::initializer_list<std::string>{op, arg0});
    }

    AsmInst &append(std::string op, std::string arg0, std::string arg1) {
        return _insts.emplace_back(
            std::initializer_list<std::string>{op, arg0, arg1});
    }

    AsmInst &append(std::string op, std::string arg0, std::string arg1,
                    std::string arg2) {
        return _insts.emplace_back(
            std::initializer_list<std::string>{op, arg0, arg1, arg2});
    }

    AsmInst &append(std::string label) {
        return _insts.emplace_back(std::move(label));
    }

    void clear() { _insts.clear(); }

    void optimize(bool minimum_stack);
    void emit(std::ostream &out) const;

  private:
    // Eliminate redundant load immediate.
    void _eliminate_immediate();

    // Make load following store to move.
    void _weaken_load();

    // Eliminate move to the same register.
    // After _weaken_load.
    void _eliminate_move();

    // Eliminate redundant jump.
    void _eliminate_jump();

    // Simplify integer compare and branch to single branch.
    // After _eliminate_jump.
    void _simplify_cmp_branch();

    // Weaken branch to remove else jump.
    // After _simplify_cmp_branch.
    void _weaken_branch();

    // Remove redundant mv in arithmetic instructions.
    void _weaken_arithmetic();

    // Remove redundant stack management for leaf function.
    void _eliminate_entry_exit();

    bool _is_temp_reg(const std::string &reg) const;

    void _slide(iterator begin, iterator end, int window_size, bool inst_only,
                std::vector<std::vector<std::string>> patterns,
                std::function<void(std::deque<iterator> &)> callback);

  private:
    std::list<AsmInst> _insts;
};

} // namespace target