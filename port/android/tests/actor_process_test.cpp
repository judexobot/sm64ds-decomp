// Execute the original, PC-transformed init/behavior Process wrappers using
// native callback signatures. Observation callbacks exercise all result paths.
#include <cassert>
#include <cstdio>
#include <vector>

extern "C" int func_0204335c(void *);
extern "C" int func_02043288(void *);
struct Actor { void **table; int value; };
static Actor *expected;
static int before_result, main_result, after_code;
static std::vector<int> order;
static int before(void *self) {
    assert(self == expected); order.push_back(1); return before_result;
}
static int main_slot(void *self) {
    assert(self == expected); order.push_back(2); return main_result;
}
static void after(void *self, unsigned code) {
    assert(self == expected); order.push_back(3); after_code = code;
}
int main() {
    void *table[18]{};
    Actor actor{table, 42}; expected = &actor;
    for (int kind = 0; kind < 2; ++kind) {
        int base = kind ? 6 : 0;
        table[base] = reinterpret_cast<void *>(main_slot);
        table[base + 1] = reinterpret_cast<void *>(before);
        table[base + 2] = reinterpret_cast<void *>(after);
        auto process = kind ? func_02043288 : func_0204335c;
        for (int result : {-1, 0, 1}) {
            order.clear(); before_result = 1; main_result = result;
            assert(process(&actor) == result);
            assert((order == std::vector<int>{1,2,3}));
            assert(after_code == (result == -1 ? 3 : result == 1 ? 2 : 1));
        }
        order.clear(); before_result = 0;
        assert(process(&actor) == 0 && after_code == 0);
        assert((order == std::vector<int>{1,3}));
    }
    std::puts("original actor init/behavior Process: native arguments, gates and all result paths PASS");
}
