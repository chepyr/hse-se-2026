#include "shell/ShellApp.hpp"
#include <string>
#include "shell/Parse.hpp"
#include "shell/Utils.hpp"

namespace shell {

// Helper to check if there's an unclosed quote
static bool hasUnclosedQuote(const std::string &str) {
    bool in_single = false;
    bool in_double = false;

    for (char ch : str) {
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
        } else if (ch == '"' && !in_single) {
            in_double = !in_double;
        }
    }

    return in_single || in_double;
}

ShellApp::ShellApp(std::istream &in, std::ostream &out, std::ostream &err)
    : in_(in), out_(out), err_(err), env_(), executor_() {
}

void ShellApp::printPromptIfInteractive() const {
    if (Utils::isInteractiveStdin()) {
        out_ << "> " << std::flush;
    }
}

int ShellApp::run() {
    std::string line;
    while (true) {
        printPromptIfInteractive();

        if (!std::getline(in_, line)) {
            // EOF: exit gracefully with last exit code (common shell behavior
            // is 0/last)
            return last_exit_code_;
        }

        // Support multi-line input if quote is not closed
        while (hasUnclosedQuote(line)) {
            std::string next_line;
            if (Utils::isInteractiveStdin()) {
                out_ << "  " << std::flush;  // Continuation prompt
            }
            if (!std::getline(in_, next_line)) {
                break;  // EOF while in multi-line
            }
            line += '\n' + next_line;
        }

        ParsedLine parsed = parseLine(line, env_);
        IOStreams io{in_, out_, err_};

        CommandResult res =
            executor_.execute(parsed, env_, io, last_exit_code_);
        last_exit_code_ = res.exit_code;

        if (res.request_exit) {
            return res.exit_code;
        }
    }
}

}  // namespace shell
