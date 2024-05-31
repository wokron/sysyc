#include "error.h"

#include <iostream>

static bool _has_error = false;

bool has_error() { return _has_error; }

void error(int lineno, const std::string &msg) {
    _has_error = true;
    std::cerr << lineno << ": " << msg << std::endl;
}
