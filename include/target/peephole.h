#include <initializer_list>
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

    const std::string &op() const { return args[0]; }
    void op(const std::string &op) { args[0] = op; }
    const std::string &arg0() const { return args[1]; }
    void arg0(const std::string &arg) { args[1] = arg; }
    const std::string &arg1() const { return args[2]; }
    void arg1(const std::string &arg) { args[2] = arg; }
    const std::string &arg2() const { return args[3]; }

    void swap(int i, int j) {
        if (i < 0 || i >= args.size() || j < 0 || j >= args.size()) {
            return;
        }
        std::swap(args[i], args[j]);
    }
};

class PeepholeBuffer {
  public:
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
    std::vector<AsmInst> _insts;
};

} // namespace target