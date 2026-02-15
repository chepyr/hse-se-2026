#include "shell/Tokenizer.hpp"
#include "shell/Environment.hpp"
#include <cctype>

namespace shell {

// Helper: check if character can be part of a variable name
static bool isVarChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Helper: expand variable reference
static std::string expandVar(
    const std::string &line,
    size_t &i,
    const Environment &env
) {
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

TokenizeResult Tokenizer::tokenize(
    const std::string &line,
    const Environment &env
) {
    TokenizeResult res;
    res.ok = false;

    enum class State { Normal, InSingle, InDouble };
    State st = State::Normal;

    std::vector<std::string> tokens;
    std::string cur;
    bool have_token = false;

    auto push_token = [&]() {
        if (have_token) {
            tokens.push_back(cur);
            cur.clear();
            have_token = false;
        }
    };

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (st == State::Normal) {
            // Check for pipe separator
            if (ch == '|') {
                push_token();
                tokens.push_back("|");
                continue;
            }
            
            // Check for quotes
            if (ch == '\'') {
                st = State::InSingle;
                have_token = true;
                continue;
            }
            if (ch == '"') {
                st = State::InDouble;
                have_token = true;
                continue;
            }
            
            // Check for whitespace
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                push_token();
                continue;
            }
            
            // Check for variable substitution
            if (ch == '$') {
                std::string value = expandVar(line, i, env);
                cur.append(value);
                have_token = true;
                --i;  // expandVar leaves i one position past last char
                continue;
            }
            
            // Regular character
            cur.push_back(ch);
            have_token = true;
            
        } else if (st == State::InSingle) {
            // Inside single quotes - no substitution
            if (ch == '\'') {
                st = State::Normal;
                continue;
            }
            cur.push_back(ch);
            have_token = true;
            
        } else {  // InDouble
            // Inside double quotes - with substitution
            if (ch == '"') {
                st = State::Normal;
                continue;
            }
            
            // Check for variable substitution in double quotes
            if (ch == '$') {
                std::string value = expandVar(line, i, env);
                cur.append(value);
                have_token = true;
                --i;  // expandVar leaves i one position past last char
                continue;
            }
            
            cur.push_back(ch);
            have_token = true;
        }
    }

    if (st != State::Normal) {
        res.error = "Unterminated quote";
        return res;
    }

    push_token();

    res.ok = true;
    res.tokens = std::move(tokens);
    return res;
}

}  // namespace shell
