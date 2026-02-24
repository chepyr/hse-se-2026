/**
 * @file Grep.cpp
 * @brief Built-in grep: regex search with -w, -i, -A. Uses CLI11 for option
 * parsing.
 */

#include "shell/Grep.hpp"
#include <CLI/CLI.hpp>
#include <cctype>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace shell {

namespace {

/** Word character: letter, digit, or underscore (per std::isalnum + '_'). */
static bool isWordChar(unsigned char c) {
    return std::isalnum(c) != 0 || c == '_';
}

/** Check that the match is on word boundaries (for -w). */
static bool isWholeWordMatch(
    const std::string &line,
    std::ptrdiff_t start,
    std::ptrdiff_t length
) {
    if (start > 0 && isWordChar(static_cast<unsigned char>(line[start - 1]))) {
        return false;
    }
    std::size_t end = static_cast<std::size_t>(start + length);
    if (end < line.size() &&
        isWordChar(static_cast<unsigned char>(line[end]))) {
        return false;
    }
    return true;
}

/** Read lines from stream into vector; returns false on read error. */
static bool readLines(std::istream &in, std::vector<std::string> &lines) {
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return !in.bad();
}

/** Run grep on a list of lines; fill to_print with line indices to output. */
static void matchLines(
    const std::vector<std::string> &lines,
    const std::regex &re,
    bool word_only,
    int after_context,
    std::set<std::size_t> &to_print
) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::smatch m;
        const std::string &line = lines[i];
        if (!std::regex_search(line, m, re)) {
            continue;
        }
        if (word_only && !isWholeWordMatch(line, m.position(), m.length())) {
            continue;
        }
        for (int k = 0; k <= after_context; ++k) {
            std::size_t idx = i + static_cast<std::size_t>(k);
            if (idx < lines.size()) {
                to_print.insert(idx);
            }
        }
    }
}

}  // namespace

CommandResult runGrep(const std::vector<std::string> &argv, IOStreams io) {
    if (argv.empty()) {
        return {-1, false};
    }

    CLI::App app{"grep - search for regex in lines"};
    bool word_only = false;
    bool case_insensitive = false;
    int after_context = 0;
    std::string pattern;
    std::vector<std::string> files;

    app.add_flag("-w,--word-regexp", word_only, "match only whole words");
    app.add_flag("-i,--ignore-case", case_insensitive, "case-insensitive");
    app.add_option("-A,--after-context", after_context, "lines after match")
        ->type_name("NUM")
        ->default_val(0);
    app.add_option("pattern", pattern, "regex pattern")->required();
    app.add_option("files", files, "input files (default: stdin)");

    app.name("grep");
    try {
        std::vector<std::string> args(argv.begin(), argv.end());
        std::vector<char *> argv_ptrs;
        argv_ptrs.reserve(args.size() + 1);
        for (auto &s : args) {
            argv_ptrs.push_back(s.data());
        }
        argv_ptrs.push_back(nullptr);
        app.parse(static_cast<int>(args.size()), argv_ptrs.data());
    } catch (const CLI::ParseError &e) {
        io.err << "grep: " << e.what() << '\n';
        return {2, false};
    }

    if (after_context < 0) {
        io.err << "grep: -A argument must be non-negative\n";
        return {2, false};
    }

    std::regex::flag_type re_flags = std::regex::ECMAScript;
    if (case_insensitive) {
        re_flags |= std::regex::icase;
    }

    std::regex re;
    try {
        re = std::regex(pattern, re_flags);
    } catch (const std::regex_error &e) {
        io.err << "grep: invalid regex: " << e.what() << '\n';
        return {2, false};
    }

    std::vector<std::string> all_lines;
    if (files.empty()) {
        if (!readLines(io.in, all_lines)) {
            io.err << "grep: read error\n";
            return {1, false};
        }
    } else {
        for (const std::string &path : files) {
            std::ifstream f(path, std::ios::binary);
            if (!f) {
                io.err << "grep: " << path << ": cannot open file\n";
                return {1, false};
            }
            if (!readLines(f, all_lines)) {
                io.err << "grep: " << path << ": read error\n";
                return {1, false};
            }
        }
    }

    std::set<std::size_t> to_print;
    matchLines(all_lines, re, word_only, after_context, to_print);

    for (std::size_t i = 0; i < all_lines.size(); ++i) {
        if (to_print.count(i) != 0) {
            io.out << all_lines[i] << '\n';
        }
    }

    return {to_print.empty() ? 1 : 0, false};
}

}  // namespace shell
