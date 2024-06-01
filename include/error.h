#pragma once

#include <string>

/**
 * @brief Check if there is any error during the compilation
 * @return true if there is any error, false otherwise
 */
bool has_error();

/**
 * @brief Print an error message
 * @param lineno The line number where the error occurs
 * @param msg The error message
 * @note This function will set the global flag indicating that there is an
 * error
 */
void error(int lineno, const std::string &msg);
