#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern "C" void port_asset_root_refuse(const char *);
extern "C" void port_asset_mismatch_refuse(const char *);
static std::string read(const std::string &name) {
    std::ifstream in(name);
    return std::string(std::istreambuf_iterator<char>(in), {});
}
static void run_child(bool mismatch) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        if (mismatch) port_asset_mismatch_refuse("want sha = abcd, got sha = dead");
        else port_asset_root_refuse("build/assets/romdata.bin");
        _exit(3); // refusal MUST never return
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 2);
}
int main() {
    char tmp[] = "/tmp/sm64ds-asset-error-XXXXXX";
    char *root = mkdtemp(tmp);
    assert(root);
    assert(setenv("SM64DS_ASSET_ROOT", root, 1) == 0);
    run_child(false);
    const std::string error = std::string(root) + "/startup_error.txt";
    assert(read(error).find("build/assets/romdata.bin") != std::string::npos);
    run_child(true);
    assert(read(error).find("different port build") != std::string::npos);
    assert(read(error).find("abcd") != std::string::npos);
    // No repository fallback and no write to a user-controlled invalid path.
    assert(unsetenv("SM64DS_ASSET_ROOT") == 0);
    run_child(false);
    std::puts("Android PC-ROM-clean refusal: wrong build, missing root, exit=2 passed");
}
