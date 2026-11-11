// Execute the original src/ transition, using synthetic save bytes. Only the
// external fader, scene-request and controller consumers are observation hooks.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint8_t data_0209f2d8 = 1, data_0209caa0[0x32c];
uint8_t data_02092128, data_02092124, data_0209f228;
uint8_t data_0209f2f4, data_0209f254, data_0208a0e0;
uint16_t data_02092144[4], data_0209f5e8[7];
int8_t data_02092114, data_02092110 = -1;
int8_t data_02092118 = 2, data_0209211c = 2, data_0209f2f8 = 1;
uint8_t data_0209f268, data_0209f220 = 5, data_0209f1f0;
uint8_t data_0209f26c, data_0209f200, data_0209f270, data_0209f24c;

static int step, controller;
void _ZN8dScene_c20SetAndStopColorFaderEv(void) { assert(step++ == 0); }
void SetNextStar(void) {
    assert(step++ == 1);
    assert(data_02092110 == 1 && data_0209f268 == 0);
    assert(data_0209f1f0 == 1);
}
int SublevelToLevel(int level) { return level; }
void InitControllerMode(int player, int health) {
    assert(step == 2 && player == controller++ && health == 6);
    assert(data_02092144[player] == 0x880);
}
void func_020308d0(void) {
    assert(step++ == 2 && controller == 4 && data_0208a0e0 == 1);
}
void _ZN8dScene_c14StartSceneFadeEjjt(unsigned scene, unsigned param,
                                    uint16_t color) {
    assert(step++ == 3);
    assert(scene == 3 && param == 0 && color == 0x7fff);
    assert(data_02092128 == 3 && data_02092124 == 6 && !data_0209f228);
}
void StartFile(int8_t level, uint8_t entrance);
int main(void) {
    data_0209caa0[0x41] = 3;
    data_0209caa0[0x42] = 6;
    data_0209caa0[0x32b] = 0xa5;
    data_0209f5e8[6] = 0xffff;
    StartFile(1, 0);
    assert(step == 4 && controller == 4);
    assert(!data_0209f2d8 && data_0209f2f4 == 4 && !data_0209f254);
    assert(data_02092114 == -1 && data_02092118 == -1 && data_0209211c == -1);
    assert(data_0209f5e8[6] == 0 && data_0209caa0[0x32b] == 0xa5);
    puts("original StartFile -> LoadLevel -> four controllers -> Stage fade: PASS (synthetic save)");
}
