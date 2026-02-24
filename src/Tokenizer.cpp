#include "shell/Tokenizer.hpp"
#include <cctype>
#include "shell/Environment.hpp"

namespace shell {

// Helper: check if character can be part of a variable name
static bool isVarChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Helper: expand variable reference
static std::string
expandVar(const std::string &line, size_t &i, const Environment &env) {
    // We're at $ character
    ++i;
    if (i >= line.size()) {
        return "$";  // Trailing $
    }

    // Check for ${...}
    if (line[i] == '{') {
        ++i;
        std::string varname;
        while (i < line.size() && line[i] != '}') {
            varname.push_back(line[i]);
            ++i;
        }
        if (i < line.size() && line[i] == '}') {
            ++i;  // Skip closing }
        }
        return env.get(varname);
    }

    // Simple $VAR form
    std::string varname;
    while (i < line.size() && isVarChar(line[i])) {
        varname.push_back(line[i]);
        ++i;
    }

    if (varname.empty()) {
        return "$";  // Just $ with no var name
    }

    return env.get(varname);
}

TokenizeResult
Tokenizer::tokenize(const std::string &line, const Environment &env) {
    TokenizeResult result;
    result.ok = false;

    enum class State { Normal, InSingle, InDouble };
    State state = State::Normal;

    std::vector<std::string> tokens;
    std::string current_token;
    bool has_token = false;

    auto flush_token = [&]() {
        if (has_token) {
            tokens.push_back(current_token);
            current_token.clear();
            has_token = false;
        }
    };

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (state == State::Normal) {
            if (ch == '|') {
                flush_token();
                tokens.push_back("|");
                continue;
            }

            if (ch == '\'') {
                state = State::InSingle;
                has_token = true;
                continue;
            }
            if (ch == '"') {
                state = State::InDouble;
                has_token = true;
                continue;
            }

            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                flush_token();
                continue;
            }

            if (ch == '$') {
                std::string expanded = expandVar(line, i, env);
                current_token.append(expanded);
                has_token = true;
                --i;
                continue;
            }

            current_token.push_back(ch);
            has_token = true;

        } else if (state == State::InSingle) {
            if (ch == '\'') {
                state = State::Normal;
                continue;
            }
            current_token.push_back(ch);
            has_token = true;

        } else {
            if (ch == '"') {
                state = State::Normal;
                continue;
            }

            if (ch == '$') {
                std::string expanded = expandVar(line, i, env);
                current_token.append(expanded);
                has_token = true;
                --i;
                continue;
            }

            current_token.push_back(ch);
            has_token = true;
        }
    }

    if (state != State::Normal) {
        result.error = "Unterminated quote";
        return result;
    }

    flush_token();

    result.ok = true;
    result.tokens = std::move(tokens);
    return result;
}

}  // namespace shell
