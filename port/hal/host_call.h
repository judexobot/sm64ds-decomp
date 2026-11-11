#pragma once

// x86 fastcall uses EDX as a dummy between `this` and stack arguments.
// AAPCS passes `this` in r0 and the FIRST real argument in r1. Keeping the
// dummy on ARM would silently shift every callback with arguments.
#if defined(SM64DS_PLATFORM_ANDROID)
#if !defined(__arm__) || defined(__aarch64__)
#error The game host callbacks require 32-bit ARM AAPCS
#endif
#define __fastcall
#define __cdecl
#define PORT_EDX
#define PORT_EDX_NAMED(name)
#define PORT_EDX_ARG(value)
#else
#define PORT_EDX , void *
#define PORT_EDX_NAMED(name) , void *name
#define PORT_EDX_ARG(value) , value
#endif
