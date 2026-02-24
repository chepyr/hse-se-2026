#include "shell/ShellApp.hpp"
#include <csignal>
#include <string>
#include "shell/Parse.hpp"
#include "shell/Utils.hpp"

namespace shell {

#if !defined(_WIN32)
static volatile sig_atomic_t g_sigint_received = 0;

static void sigint_handler(int) {
    g_sigint_received = 1;
}
#endif

static bool hasUnclosedQuote(const std::string &input) {
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (char ch : input) {
        if (ch == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (ch == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }
    }

    return in_single_quote || in_double_quote;
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
#if !defined(_WIN32)
    if (Utils::isInteractiveStdin()) {
        struct sigaction sa{};
        sa.sa_handler = sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;  // no SA_RESTART: let read() return on Ctrl+C
        sigaction(SIGINT, &sa, nullptr);
    }
#endif

    std::string line;
    while (true) {
        printPromptIfInteractive();

        if (!std::getline(in_, line)) {
#if !defined(_WIN32)
            if (Utils::isInteractiveStdin() && g_sigint_received) {
                g_sigint_received = 0;
                in_.clear();
                out_ << '\n' << std::flush;
                continue;
            }
#endif
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
#if !defined(_WIN32)
                if (Utils::isInteractiveStdin() && g_sigint_received) {
                    g_sigint_received = 0;
                    in_.clear();
                    line.clear();
                    out_ << '\n' << std::flush;
                }
#endif
                break;  // EOF or Ctrl+C while in multi-line
            }
            line += '\n' + next_line;
        }

        ParsedLine parsed = parseLine(line, env_);
        IOStreams io{in_, out_, err_};

        CommandResult result =
            executor_.execute(parsed, env_, io, last_exit_code_);
        last_exit_code_ = result.exit_code;

        if (result.request_exit) {
            return result.exit_code;
        }
    }
}

}  // namespace shell
