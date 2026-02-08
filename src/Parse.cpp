#include "shell/Parse.hpp"
#include <cstddef>
#include "shell/Tokenizer.hpp"
#include "shell/Utils.hpp"

namespace shell {

static bool isAssignmentOnlyToken(
    const std::string &tok,
    std::string &name,
    std::string &value
) {
    const std::size_t pos = tok.find('=');
    if (pos == std::string::npos) {
        return false;
    }
    if (pos == 0) {
        return false;
    }

    name = tok.substr(0, pos);
    value = tok.substr(pos + 1);

    return Utils::isValidEnvName(name);
}

ParsedLine parseLine(const std::string &line) {
    ParsedLine parsed;
    parsed.ok = false;

    auto t = Tokenizer::tokenize(line);
    if (!t.ok) {
        parsed.error = t.error;
        return parsed;
    }

    if (t.tokens.empty()) {
        parsed.ok = true;
        parsed.is_empty = true;
        return parsed;
    }

    // HW2: support assignment-only form NAME=VALUE as a single token line.
    if (t.tokens.size() == 1) {
        std::string name, value;
        if (isAssignmentOnlyToken(t.tokens[0], name, value)) {
            parsed.ok = true;
            parsed.is_assignment_only = true;
            parsed.assign_name = std::move(name);
            parsed.assign_value = std::move(value);
            return parsed;
        }
    }

    parsed.ok = true;
    parsed.argv = std::move(t.tokens);
    return parsed;
}

}  // namespace shell
