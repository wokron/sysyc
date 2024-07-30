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
    std::vector<std::string> args;

    AsmInst(std::initializer_list<std::string> args) : type(INST), args(args) {}
    AsmInst(std::string label) : type(LABEL) { args.push_back(label); }

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

    void append(std::string op, std::string arg0) {
        _insts.emplace_back(std::initializer_list<std::string>{op, arg0});
    }

    void append(std::string op, std::string arg0, std::string arg1) {
        _insts.emplace_back(std::initializer_list<std::string>{op, arg0, arg1});
    }

    void append(std::string op, std::string arg0, std::string arg1,
                std::string arg2) {
        _insts.emplace_back(
            std::initializer_list<std::string>{op, arg0, arg1, arg2});
    }

    void append(std::string label) { _insts.emplace_back(std::move(label)); }

    void clear() { _insts.clear(); }

    void optimize();
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

    void _slide(iterator begin, iterator end, int window_size, bool inst_only,
                std::vector<std::vector<std::string>> patterns,
                std::function<void(std::deque<iterator> &)> callback);

  private:
    std::list<AsmInst> _insts;
};

} // namespace target