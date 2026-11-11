// Portable ROM-clean asset-root refusal for the Android game flow.
// This is the native equivalent of hal/asset_root_refuse.cpp, which must
// remain Win32 for the PC release. It never substitutes a developer checkout,
// never loads mismatched Nintendo resources, and leaves a readable report in
// the app-private game directory for Android's launcher to inspect.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool safe_private_root(const char *path) {
    if (!path || path[0] != '/' || std::strlen(path) > 500 ||
        std::strstr(path, "/../") || std::strstr(path, "/./") ||
        path[std::strlen(path)-1] == '/')
        return false;
    struct stat st{};
    return lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
void report(const char *title, const char *detail) {
    const char *root = std::getenv("SM64DS_ASSET_ROOT");
    std::string text = std::string("Super Mario 64 DS could not start.\n") +
        title + "\n" + (detail ? detail : "(no detail)") + "\n";
    std::fprintf(stderr, "[sm64ds-android-assets] %s", text.c_str());
    std::fflush(stderr);
    // A missing asset root cannot safely be guessed. If there is no valid
    // app-private path, stderr / Android log is the entire report.
    if (!safe_private_root(root)) return;
    const std::string path = std::string(root) + "/startup_error.txt";
    const std::string temp = path + ".part";
    std::FILE *f = std::fopen(temp.c_str(), "wb");
    if (!f) return;
    const bool wrote = std::fwrite(text.data(), 1, text.size(), f) ==
                       text.size() && std::fflush(f) == 0 &&
                       fsync(fileno(f)) == 0;
    const int closed = std::fclose(f);
    if (!wrote || closed != 0 || std::rename(temp.c_str(), path.c_str()) != 0)
        std::remove(temp.c_str());
}
}
extern "C" void port_asset_root_refuse(const char *wanted) {
    report("The application's private game-data directory was not set.",
           wanted);
    // Original PC loader refuses with process status 2, not an abort that
    // looks like a memory-safety crash. Caller never regains control.
    std::exit(2);
}
extern "C" void port_asset_mismatch_refuse(const char *detail) {
    report("Game resources belong to a different port build. "
           "Re-import this game's own EU ROM with the matching PC kit recipe.",
           detail);
    std::exit(2);
}
