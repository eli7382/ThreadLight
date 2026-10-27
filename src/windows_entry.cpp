#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

int main(int argc, char** argv);

namespace {
std::string toUtf8(const wchar_t* value) {
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            result.data(), length, nullptr, nullptr) <= 0) {
        return {};
    }
    result.resize(static_cast<std::size_t>(length - 1));
    return result;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int wideArgc = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (wideArgv == nullptr) return 127;

    std::vector<std::string> args;
    std::vector<char*> argv;
    args.reserve(static_cast<std::size_t>(wideArgc));
    argv.reserve(static_cast<std::size_t>(wideArgc));
    bool valid = true;
    for (int i = 0; i < wideArgc; ++i) {
        args.push_back(toUtf8(wideArgv[i]));
        if (args.back().empty() && wideArgv[i][0] != L'\0') valid = false;
    }
    LocalFree(wideArgv);
    if (!valid || args.empty()) return 127;
    for (auto& arg : args) argv.push_back(arg.data());
    return main(static_cast<int>(argv.size()), argv.data());
}
#endif
