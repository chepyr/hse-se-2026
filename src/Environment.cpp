#include "shell/Environment.hpp"
#include <cstdlib>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

#if !defined(_WIN32)
extern char **environ;
#endif

namespace shell {

static void loadProcessEnvironment(
    std::unordered_map<std::string, std::string> &out
) {
#if defined(_WIN32)
    // GetEnvironmentStringsW returns a block of null-terminated strings
    // terminated by an extra null.
    LPWCH env_block = ::GetEnvironmentStringsW();
    if (!env_block) {
        return;
    }

    for (LPWCH p = env_block; *p != L'\0';) {
        std::wstring ws(p);
        p += ws.size() + 1;

        // Skip weird entries like "=C:=C:\..."
        if (!ws.empty() && ws[0] == L'=') {
            continue;
        }

        auto pos = ws.find(L'=');
        if (pos == std::wstring::npos) {
            continue;
        }

        std::wstring wkey = ws.substr(0, pos);
        std::wstring wval = ws.substr(pos + 1);

        // Convert to UTF-8 (best effort).
        auto to_utf8 = [](const std::wstring &w) -> std::string {
            if (w.empty()) {
                return {};
            }
            int size_needed = ::WideCharToMultiByte(
                CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr,
                nullptr
            );
            if (size_needed <= 0) {
                return {};
            }
            std::string s(size_needed, '\0');
            ::WideCharToMultiByte(
                CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), size_needed,
                nullptr, nullptr
            );
            return s;
        };

        std::string key = to_utf8(wkey);
        std::string val = to_utf8(wval);
        if (!key.empty()) {
            out[key] = val;
        }
    }

    ::FreeEnvironmentStringsW(env_block);
#else
    if (!environ) {
        return;
    }

    for (char **p = environ; *p != nullptr; ++p) {
        std::string_view sv(*p);
        auto eq = sv.find('=');
        if (eq == std::string_view::npos || eq == 0) {
            continue;
        }
        std::string key(sv.substr(0, eq));
        std::string val(sv.substr(eq + 1));
        out[key] = val;
    }
#endif
}

Environment::Environment() {
    loadProcessEnvironment(vars_);
}

void Environment::set(const std::string &name, const std::string &value) {
    vars_[name] = value;
}

std::vector<std::string> Environment::snapshot() const {
    std::vector<std::string> out;
    out.reserve(vars_.size());
    for (const auto &[k, v] : vars_) {
        out.push_back(k + "=" + v);
    }
    return out;
}

}  // namespace shell
