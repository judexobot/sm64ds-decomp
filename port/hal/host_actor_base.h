#pragma once

#if defined(SM64DS_PLATFORM_ANDROID)
extern "C" int _ZN7fBase_c9Virtual34Ejj(void *, unsigned, unsigned);
extern "C" int _ZN7fBase_c9Virtual38Ejj(void *, unsigned, unsigned);
#endif

// PC's historical ActorBase spelling is a linker alias for fBase_c. ELF
// must reach the original symbol explicitly; it has no MSVC alias directives.
struct ActorBase {
#if defined(SM64DS_PLATFORM_ANDROID)
    int Virtual34(unsigned a, unsigned b)
    { return _ZN7fBase_c9Virtual34Ejj(this, a, b); }
    int Virtual38(unsigned a, unsigned b)
    { return _ZN7fBase_c9Virtual38Ejj(this, a, b); }
#else
    int Virtual34(unsigned a, unsigned b);
    int Virtual38(unsigned a, unsigned b);
#endif
};
