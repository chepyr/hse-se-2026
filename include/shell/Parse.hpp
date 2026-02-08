#pragma once
#include <string>
#include <vector>

namespace shell {

/**
 * @brief Parsed representation of a single input line for HW2.
 *
 * HW2 supports only:
 * - either an assignment-only line: NAME=VALUE
 * - or a single command (argv)
 */
struct ParsedLine final {
    /** @brief True if parsing succeeded. */
    bool ok = false;

    /** @brief Error message if ok==false. */
    std::string error;

    /** @brief True if line is empty or whitespace only. */
    bool is_empty = false;

    /** @brief True if the line is a single environment assignment NAME=VALUE.
     */
    bool is_assignment_only = false;

    /** @brief Assignment variable name (if is_assignment_only). */
    std::string assign_name;

    /** @brief Assignment value (if is_assignment_only). */
    std::string assign_value;

    /** @brief Command argv (if not assignment and not empty). */
    std::vector<std::string> argv;
};

/**
 * @brief Parses a single input line into ParsedLine.
 * @param line Raw input line.
 * @return ParsedLine structure.
 */
ParsedLine parseLine(const std::string &line);

}  // namespace shell
