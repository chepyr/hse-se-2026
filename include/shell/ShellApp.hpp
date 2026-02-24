#pragma once
#include <istream>
#include <ostream>
#include <string>
#include "shell/Environment.hpp"
#include "shell/Executor.hpp"

namespace shell {

/**
 * @brief Interactive shell application (REPL).
 */
class ShellApp final {
public:
    /**
     * @brief Constructs a ShellApp with given input/output streams.
     * @param in Input stream (typically std::cin).
     * @param out Output stream (typically std::cout).
     * @param err Error stream (typically std::cerr).
     */
    ShellApp(std::istream &in, std::ostream &out, std::ostream &err);

    /**
     * @brief Runs the REPL loop until exit is requested or input ends.
     * @return Exit code of the last executed command (or 0 on clean EOF).
     */
    int run();

private:
    std::istream &in_;
    std::ostream &out_;
    std::ostream &err_;

    Environment env_;
    Executor executor_;
    int last_exit_code_ = 0;

    void printPromptIfInteractive() const;
};

}  // namespace shell
