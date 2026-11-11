.syntax unified
.thumb
.text
.p2align 2
.global sm64ds_arm_switch
.type sm64ds_arm_switch, %function
.thumb_func
sm64ds_arm_switch:
    push.w {r4-r11, lr}
    mov r2, sp
    str r2, [r0]
    mov sp, r1
    pop.w {r4-r11, pc}
.size sm64ds_arm_switch, .-sm64ds_arm_switch
