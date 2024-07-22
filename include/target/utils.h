#pragma once

#include <string>

namespace target {

inline std::string regno2string(int number) {
    const char *regnames[] = {
        "zero", "ra",  "sp",   "gp",   "tp",  "t0",  "t1",   "t2",
        "s0",   "s1",  "a0",   "a1",   "a2",  "a3",  "a4",   "a5",
        "a6",   "a7",  "s2",   "s3",   "s4",  "s5",  "s6",   "s7",
        "s8",   "s9",  "s10",  "s11",  "t3",  "t4",  "t5",   "t6",
        "ft0",  "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
        "fs0",  "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
        "fa6",  "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
        "fs8",  "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11",
    };
    if (number == -1) {
        return "spill";
    } else if (number == -2) {
        return "stack";
    } else if (number == -3) {
        return "no_register";
    } else if (number < 0 || number >= 64) {
        return "unknown";
    }
    return regnames[number];
}

inline std::string build(std::string op, std::string rd, std::string rs1,
                         std::string rs2) {
    return op + " " + rd + ", " + rs1 + ", " + rs2;
}

inline std::string build(std::string op, std::string rd, std::string rs1) {
    return op + " " + rd + ", " + rs1;
}

inline std::string build(std::string op, std::string rd) {
    return op + " " + rd;
}

// This method should only be called when there is no register allocation.
inline int get_temp_reg() {
    static int choice = 0;
    choice ^= 1;
    return choice ? 14 : 15;
}

} // namespace target
