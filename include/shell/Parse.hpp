#pragma once
#include <string>
#include <vector>

namespace shell {

/**
 * @brief Single command in a pipeline.
 *
 * Each command consists of arguments (argv-style).
 */
struct CommandSpec final {
    /** @brief Command arguments (argv-style). */
    std::vector<std::string> argv;
};

/**
 * @brief Parsed representation of a single input line.
 *
 * Supports:
 * - assignment-only line: NAME=VALUE
 * - single command: cmd arg1 arg2
 * - pipeline: cmd1 | cmd2 | cmd3
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

    /** @brief Commands in the pipeline. 
     * For a single command, pipeline.size() == 1.
     * For a pipeline like "cmd1 | cmd2", pipeline.size() == 2.
     */
    std::vector<CommandSpec> pipeline;
};

/**
 * @brief Parses a single input line into ParsedLine.
 * @param line Raw input line.
 * @param env Environment for variable substitution.
 * @return ParsedLine structure.
 */
ParsedLine parseLine(const std::string &line, const class Environment &env);

}  // namespace shell
