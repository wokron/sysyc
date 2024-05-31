#pragma once

#include <string>

bool has_error();

void error(int lineno, const std::string &msg);
