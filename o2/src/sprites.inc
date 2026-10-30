; sprites.inc -- spot_mark: the spotlight's marker (sprite 0, screen.inc
; spr_table), for a state (page 3) and the stick (page 4).

; spot_mark: sprite 0 beside seat SPOT's initial -- us on row 6, seat k on
; row (k-1)/2, on the right when k is even -- or below the screen if there's
; no such seat. Clobbers A, r0-r4.
spot_mark:
    mov r0,#SPOT
    movx a,@r0
    mov r3,a
    cpl a
    mov r1,a
    mov r0,#NSEAT
    movx a,@r0
    add a,r1                    ; carry: NSEAT > SPOT
    mov r4,#MARKX
    mov a,#SPR_OFF
    jnc sm_put
    mov a,r3
    mov r1,#ROW_US
    jz sm_y
    dec a
    jb0 sm_r
    jmp sm_row
sm_r:
    mov r4,#MARKX+RIGHTX
sm_row:
    clr c
    rrc a
    mov r1,a
sm_y:
    mov a,r1                    ; Y_ROW0 + 24 x row
    rl a
    rl a
    rl a
    mov r3,a
    rl a
    add a,r3
    add a,#Y_ROW0+MARKDY
sm_put:
    mov r3,a
    mov r2,#SPRTAB>>8
    mov a,#SPRTAB&0FFh
    call poke_at
    mov a,r3
    call poke_b                 ; its Y, then its X
    mov a,r4
    jmp poke_b
