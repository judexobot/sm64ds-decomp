// Linux ARM CPU-test entry only: bypasses Android libc initialization.
// This file is never linked into Android game libraries or an APK.
.syntax unified
.thumb
.text
.global _start
.type _start,%function
.thumb_func
_start:
    bl main
    movs r7, #1
    svc #0
.section .note.GNU-stack,"",%progbits
