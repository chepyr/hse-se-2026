#include "shell/ExternalRunner.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace shell {

static std::wstring toWide(const std::string &s) {
    if (s.empty()) {
        return {};
    }
    int size_needed =
        ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (size_needed <= 0) {
        return {};
    }
    std::wstring w(size_needed, L'\0');
    ::MultiByteToWideChar(
        CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), size_needed
    );
    return w;
}

static std::string toUtf8(const std::wstring &w) {
    if (w.empty()) {
        return {};
    }
    int size_needed = ::WideCharToMultiByte(
        CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr
    );
    if (size_needed <= 0) {
        return {};
    }
    std::string s(size_needed, '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), size_needed, nullptr,
        nullptr
    );
    return s;
}

static std::wstring buildCommandLine(const std::vector<std::string> &argv) {
    // Minimal quoting for CreateProcessW: quote args with spaces or quotes.
    // This is not a full cmd.exe parser, but good enough for typical cases.
    std::wstring cmd;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i) {
            cmd.push_back(L' ');
        }
        std::wstring w = toWide(argv[i]);
        bool need_quotes = w.find_first_of(L" \t\"") != std::wstring::npos;
        if (!need_quotes) {
            cmd += w;
        } else {
            cmd.push_back(L'"');
            for (wchar_t ch : w) {
                if (ch == L'"') {
                    cmd += L"\\\"";
                } else {
                    cmd.push_back(ch);
                }
            }
            cmd.push_back(L'"');
        }
    }
    return cmd;
}

static std::vector<wchar_t> buildEnvironmentBlock(
    const std::vector<std::string> &env_snapshot
) {
    // Environment block: sequence of null-terminated "k=v" strings, terminated
    // by an extra null.
    std::wstring block;
    for (const auto &kv : env_snapshot) {
        std::wstring wkv = toWide(kv);
        block.append(wkv);
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    std::vector<wchar_t> buf(block.begin(), block.end());
    return buf;
}

static void pumpPipeToStream(HANDLE hRead, std::ostream &os, std::mutex &mtx) {
    constexpr DWORD kBufSize = 4096;
    char buf[kBufSize];
    DWORD n = 0;

    while (true) {
        BOOL ok = ::ReadFile(hRead, buf, kBufSize, &n, nullptr);
        if (!ok || n == 0) {
            break;
        }
        std::lock_guard<std::mutex> lk(mtx);
        os.write(buf, static_cast<std::streamsize>(n));
        os.flush();
    }
}

CommandResult ExternalRunner::run(
    const std::vector<std::string> &argv,
    const std::vector<std::string> &env_snapshot,
    IOStreams io
) {
    if (argv.empty()) {
        return {2, false};
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE outRead = nullptr, outWrite = nullptr;
    HANDLE errRead = nullptr, errWrite = nullptr;

    if (!::CreatePipe(&outRead, &outWrite, &sa, 0)) {
        io.err << "failed to create stdout pipe\n";
        return {127, false};
    }
    if (!::CreatePipe(&errRead, &errWrite, &sa, 0)) {
        ::CloseHandle(outRead);
        ::CloseHandle(outWrite);
        io.err << "failed to create stderr pipe\n";
        return {127, false};
    }

    // Ensure the read ends are NOT inherited.
    ::SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = outWrite;
    si.hStdError = errWrite;

    PROCESS_INFORMATION pi{};
    std::wstring cmdline = buildCommandLine(argv);

    // Make env block deterministic and Windows-friendly: sort by variable name.
    auto sortedEnv = env_snapshot;
    std::sort(
        sortedEnv.begin(), sortedEnv.end(),
        [](const std::string &a, const std::string &b) {
            auto keyOf = [](const std::string &s) {
                auto pos = s.find('=');
                std::string key =
                    (pos == std::string::npos) ? s : s.substr(0, pos);
                for (char &ch : key) {
                    ch = static_cast<char>(
                        std::tolower(static_cast<unsigned char>(ch))
                    );
                }
                return key;
            };
            return keyOf(a) < keyOf(b);
        }
    );

    std::vector<wchar_t> envBlock = buildEnvironmentBlock(sortedEnv);

    // CreateProcessW requires modifiable command line buffer.
    std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = ::CreateProcessW(
        nullptr, cmdBuf.data(), nullptr, nullptr,
        TRUE,  // inherit handles (pipes)
        CREATE_UNICODE_ENVIRONMENT,
        envBlock.empty() ? nullptr : envBlock.data(), nullptr, &si, &pi
    );

    // Parent doesn't need write ends.
    ::CloseHandle(outWrite);
    ::CloseHandle(errWrite);

    if (!ok) {
        DWORD err = ::GetLastError();
        io.err << "failed to start process (winerr="
               << static_cast<unsigned long>(err) << ")\n";
        ::CloseHandle(outRead);
        ::CloseHandle(errRead);
        return {127, false};
    }

    std::mutex out_mtx, err_mtx;
    std::thread t_out([&]() { pumpPipeToStream(outRead, io.out, out_mtx); });
    std::thread t_err([&]() { pumpPipeToStream(errRead, io.err, err_mtx); });

    ::WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    ::GetExitCodeProcess(pi.hProcess, &exitCode);

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);

    ::CloseHandle(outRead);
    ::CloseHandle(errRead);

    t_out.join();
    t_err.join();

    return {static_cast<int>(exitCode), false};
}

}  // namespace shell
