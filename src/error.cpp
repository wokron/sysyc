#include "error.h"

#include <iostream>

static bool has_error_flag = false;

bool has_error() { return has_error_flag; }

void error(int lineno, const std::string &msg) {
    has_error_flag = true;
    std::cerr << lineno << ": " << msg << std::endl;
}
