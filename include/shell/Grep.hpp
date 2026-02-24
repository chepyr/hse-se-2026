#pragma once
#include <string>
#include <vector>
#include "shell/CommandResult.hpp"
#include "shell/Executor.hpp"

namespace shell {

/**
 * @brief Built-in grep: search for regex pattern in lines.
 *
 * Supports -w (whole word), -i (case-insensitive), -A N (N lines after match).
 * Argument parsing is done via CLI11 (see README).
 */
CommandResult runGrep(const std::vector<std::string> &argv, IOStreams io);

}  // namespace shell
