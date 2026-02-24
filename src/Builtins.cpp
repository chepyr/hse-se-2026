#include "shell/Builtins.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include "shell/Grep.hpp"

namespace shell {

static bool parseInt(const std::string &str, int &out) {
    if (str.empty()) {
        return false;
    }
    std::size_t pos = 0;
    try {
        long value = std::stol(str, &pos, 10);
        if (pos != str.size()) {
            return false;
        }
        if (value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max()) {
            return false;
        }
        out = static_cast<int>(value);
        return true;
    } catch (const std::invalid_argument &) {
        return false;
    } catch (const std::out_of_range &) {
        return false;
    }
}

CommandResult Builtins::runIfBuiltin(
    const std::vector<std::string> &argv,
    IOStreams io,
    int last_exit_code
) {
    static constexpr int kNotBuiltin = -1;

    if (argv.empty()) {
        return {kNotBuiltin, false};
    }

    const std::string &cmd_name = argv[0];
    if (cmd_name == "echo") {
        return cmdEcho(argv, io);
    }
    if (cmd_name == "pwd") {
        return cmdPwd(io);
    }
    if (cmd_name == "cat") {
        return cmdCat(argv, io);
    }
    if (cmd_name == "wc") {
        return cmdWc(argv, io);
    }
    if (cmd_name == "grep") {
        return cmdGrep(argv, io);
    }
    if (cmd_name == "exit") {
        return cmdExit(argv, io, last_exit_code);
    }

    return {kNotBuiltin, false};
}

CommandResult
Builtins::cmdEcho(const std::vector<std::string> &argv, IOStreams io) {
    // echo prints all arguments after the command name separated by spaces.
    for (size_t i = 1; i < argv.size(); ++i) {
        if (i > 1) {
            io.out << ' ';
        }
        io.out << argv[i];
    }
    io.out << '\n';
    return {0, false};
}

CommandResult Builtins::cmdPwd(IOStreams io) {
    try {
        io.out << std::filesystem::current_path().string() << '\n';
        return {0, false};
    } catch (const std::exception &e) {
        io.err << "pwd: " << e.what() << '\n';
        return {1, false};
    }
}

CommandResult
Builtins::cmdCat(const std::vector<std::string> &argv, IOStreams io) {
    try {
        if (argv.size() == 1) {
            io.out << io.in.rdbuf();
            return {0, false};
        }

        if (argv.size() != 2) {
            io.err << "cat: expected zero or one file argument\n";
            return {2, false};
        }

        const std::string &file_path = argv[1];
        std::ifstream file_stream(file_path, std::ios::binary);
        if (!file_stream) {
            io.err << "cat: " << file_path << ": cannot open file\n";
            return {1, false};
        }

        io.out << file_stream.rdbuf();
        return {0, false};
    } catch (const std::exception &ex) {
        io.err << "cat: " << ex.what() << '\n';
        return {1, false};
    }
}

CommandResult
Builtins::cmdWc(const std::vector<std::string> &argv, IOStreams io) {
    try {
        std::istream *input = nullptr;
        std::ifstream file_stream;

        if (argv.size() == 1) {
            input = &io.in;
        } else if (argv.size() == 2) {
            const std::string &file_path = argv[1];
            file_stream.open(file_path, std::ios::binary);
            if (!file_stream) {
                io.err << "wc: " << file_path << ": cannot open file\n";
                return {1, false};
            }
            input = &file_stream;
        } else {
            io.err << "wc: expected zero or one file argument\n";
            return {2, false};
        }

        std::uint64_t line_count = 0;
        std::uint64_t word_count = 0;
        std::uint64_t byte_count = 0;

        bool in_word = false;
        char ch = '\0';
        while (input->get(ch)) {
            ++byte_count;
            if (ch == '\n') {
                ++line_count;
            }

            bool is_space = (std::isspace(static_cast<unsigned char>(ch)) != 0);
            if (is_space) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                ++word_count;
            }
        }

        io.out << line_count << ' ' << word_count << ' ' << byte_count << '\n';
        return {0, false};
    } catch (const std::exception &ex) {
        io.err << "wc: " << ex.what() << '\n';
        return {1, false};
    }
}

CommandResult Builtins::cmdExit(
    const std::vector<std::string> &argv,
    IOStreams io,
    int last_exit_code
) {
    if (argv.size() == 1) {
        return {last_exit_code, true};
    }
    if (argv.size() == 2) {
        int code = 0;
        if (!parseInt(argv[1], code)) {
            io.err << "exit: numeric argument required\n";
            return {2, true};
        }
        return {code, true};
    }
    io.err << "exit: too many arguments\n";
    return {2, false};
}

CommandResult
Builtins::cmdGrep(const std::vector<std::string> &argv, IOStreams io) {
    return runGrep(argv, io);
}

}  // namespace shell
