#include "shell/Tokenizer.hpp"
#include <cctype>

namespace shell {

TokenizeResult Tokenizer::tokenize(const std::string &line) {
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
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                push_token();
                continue;
            }
            cur.push_back(ch);
            have_token = true;
        } else if (st == State::InSingle) {
            if (ch == '\'') {
                st = State::Normal;
                continue;
            }
            cur.push_back(ch);
            have_token = true;
        } else {  // InDouble
            if (ch == '"') {
                st = State::Normal;
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
