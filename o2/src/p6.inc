; p6.inc -- page 6: the table, when a state has come, after page 3 (the
; same pass of the main loop, so the reply is still the state's): T0 (the
; street, the pot, our purse, and their stacks of chips), T1 (whose turn and
; what they did, or the hand's result; on our turn the menu is page 4's),
; the sounds, and the chips of the street's bets. Then the state is the
; screen's: LROUND, LACTV.

    PAGE_HEAD
    db e_state2&0FFh
e_state2:
    jmp on_state2

on_state2:
    call moves_copy
    call t0_draw
    call t1_state
    call cues
    mov r0,#ROWSNEW             ; a bet comes with a row redrawn: a turn
    movx a,@r0                  ; passed, a street dealt
    jz os2_c
    call chips
os2_c:
    mov r0,#ROUND
    movx a,@r0
    mov r0,#LROUND
    movx @r0,a
    mov r0,#ACTV
    movx a,@r0
    mov r0,#LACTV
    movx @r0,a
    ret

; cues: the sounds for this state, against the last: a street dealt, the
; hand won, our turn come, chips in the pot. None for the first.
; Clobbers A, r0-r7.
cues:
    mov r3,#0                   ; the pot's low byte
    mov r2,#GAME_POT
    call st_byte
    mov r5,a
    mov r0,#LPOT
    movx a,@r0
    mov r6,a
    mov a,r5
    movx @r0,a
    mov r0,#LROUND
    movx a,@r0
    mov r4,a
    inc a
    jz cu_x                     ; the first state
    mov r0,#ROUND
    movx a,@r0
    xrl a,r4
    jz cu_turn
    movx a,@r0
    jz cu_x                     ; waiting
    xrl a,#ROUND_SHOW
    jz cu_show
    mov a,#SND_DEAL
    jmp beep
cu_show:
    call won_it
    jz cu_x
    mov a,#SND_WIN
    jmp beep
cu_turn:
    mov r0,#ACTV
    movx a,@r0
    jnz cu_pot
    mov r0,#LACTV
    movx a,@r0
    jz cu_pot
    mov a,#SND_TURN
    jmp beep
cu_pot:
    mov a,r5
    xrl a,r6
    jz cu_x
    mov a,#SND_CHIPS
    jmp beep
cu_x:
    ret

; won_it: A = 1 if lastResult starts with our name. Clobbers A, r0-r3, r6.
won_it:
    mov r3,#0
    mov r2,#GAME_LASTRESULT
    mov r1,#SCNAME
wi_l:
    mov a,r1
    mov r0,a
    movx a,@r0
    jz wi_yes
    orl a,#20h
    mov r4,a
    call st_byte
    orl a,#20h
    xrl a,r4
    jnz wi_no
    inc r1
    inc r2
    jmp wi_l
wi_yes:
    mov a,#1
    ret
wi_no:
    clr a
    ret


; our_round: A = 1 if ROUND is a street (1-4), else 0. Clobbers r0.
our_round:
    mov r0,#ROUND
    movx a,@r0
    jz or_no
    add a,#256-ROUND_SHOW
    jc or_no
    mov a,#1
    ret
or_no:
    clr a
    ret

    if $ > 900h
    error "page $8 has run out"
    endif

; ===========================================================================
    org 900h
    include "disp.inc"

; ===========================================================================
    org 0A00h
    include "text.inc"

s_pre:   db "PRE",0
s_flop:  db "FLOP",0
s_turn:  db "TURN",0
s_river: db "RIVER",0

    if $ > 0B00h
    error "page $A has run out"
    endif

; ===========================================================================
    org 0B00h
    include "stbyte.inc"
    include "beep.inc"

; seat_base: r3:r2 = seat A's record in the reply. Clobbers A, r6.
seat_base:
    mov r6,a                    ; 165 + 33A (A < 8)
    swap a
    rl a
    add a,r6
    add a,#GAME_SEATS
    mov r2,a
    clr a
    addc a,#0
    mov r3,a
    ret

; put_u16: the number r5:r4 (hi:lo), in decimal, into LINEBUF at r1, no
; leading zeros; r1 advanced. Clobbers A, r0, r2, r3, r6, r7.
put_u16:
    mov r7,#0                   ; a digit has gone out
    mov r2,#10000&0FFh
    mov r3,#10000>>8
    call pu_dig
    mov r2,#1000&0FFh
    mov r3,#1000>>8
    call pu_dig
    mov r2,#100
    mov r3,#0
    call pu_dig
    mov r2,#10
    call pu_dig
    mov r7,#1                   ; the units: always
    mov r2,#1
pu_dig:                         ; r5:r4 / r3:r2 -> a digit, r5:r4 the rest
    mov r6,#'0'
pd_l:
    clr c                       ; r5:r4 - r3:r2 = r5:r4 + ~r3:~r2 + 1
    cpl c
    mov a,r2
    cpl a
    addc a,r4
    mov r0,a
    mov a,r3
    cpl a
    addc a,r5                   ; C: no borrow, it was at least r3:r2
    jnc pd_e
    mov r5,a
    mov a,r0
    mov r4,a
    inc r6
    jmp pd_l
pd_e:
    mov a,r6
    xrl a,#'0'
    orl a,r7
    jz pd_x
    mov r7,#1
    mov a,r6
    movx @r1,a
    inc r1
pd_x:
    ret

; stack: a stack of chips for r5:r4 (kept) into the sprite shape at r0: one
; from 1, two from 25, three from 100, four from 250, from the bottom, a
; line of felt between. Clobbers A, r0, r1, r6.
stack:
    mov r1,#STACK_N
    mov a,r5
    jnz sk_n
    mov r1,#0
    mov a,r4
    jz sk_n
    inc r1
    add a,#256-25
    jnc sk_n
    inc r1
    mov a,r4
    add a,#256-100
    jnc sk_n
    inc r1
    mov a,r4
    add a,#256-250
    jnc sk_n
    inc r1
sk_n:
    mov a,r1
    cpl a
    add a,#STACK_N+1
    mov r6,a                    ; the empty places above
    mov r1,#STACK_N
    mov a,#P1_VDC
    outl p1,a
sk_l:
    clr a                       ; felt
    movx @r0,a
    inc r0
    mov a,r6
    jz sk_chip
    dec r6
    clr a
    jmp sk_put
sk_chip:
    mov a,#7Eh                  ; a chip, edge on
sk_put:
    movx @r0,a
    inc r0
    djnz r1,sk_l
    mov a,#P1_MAIN
    outl p1,a
    ret

; t1_say: T1 = LINEBUF in TXCOL / TXCOL2, unless that's what it shows
; already (T1SIG: a signature of the line and its colours; a page that
; writes T1 itself sets it 0FFh). Clobbers A, r0-r7.
t1_say:
    mov r0,#LINEBUF
    mov r2,#TXT_LEN+2           ; (TXCOL, TXCOL2 follow LINEBUF)
    clr a
t1s_l:
    rl a
    mov r3,a
    movx a,@r0
    add a,r3
    inc r0
    djnz r2,t1s_l
    mov r3,a
    inc a                       ; (0FFh is "draw it": never a signature)
    jnz t1s_k
    dec r3
t1s_k:
    mov r0,#T1SIG
    movx a,@r0
    xrl a,r3
    jz t1s_x
    mov a,r3
    movx @r0,a
    mov r4,#L_BOT
    jmp say_line_c
t1s_x:
    ret

; t0_sig: r7 = r7 rotated plus reply byte r3:r2, then the same for the next
; (no u16 here straddles a 256). Clobbers A, r0, r6; r2 moved on.
t0_sig:
    call t0s_1
    inc r2
t0s_1:
    call st_byte
    xch a,r7
    rl a
    add a,r7
    mov r7,a
    ret

    if $ > 0C00h
    error "page $B has run out"
    endif

; ===========================================================================
    org 0C00h
; t1_state: T1 for this state: on our turn page 4's menu (drawn afresh);
; waiting for players, or the hand over, its result; else the spotlight's
; seat. Clobbers A, r0-r7.
t1_state:
    mov r0,#ROUND               ; a new round: the result from its start
    movx a,@r0
    mov r4,a
    mov r0,#LROUND
    movx a,@r0
    xrl a,r4
    jz t1_go
    clr a
    mov r0,#RPAGE
    movx @r0,a
    mov r0,#RPCLK
    movx @r0,a
t1_go:
    call our_round
    jnz t1_st
    jmp lr_draw
t1_st:
    mov r0,#NMOV
    movx a,@r0
    jz seat_line
    mov r0,#T1KEY
    mov a,#0FFh
    movx @r0,a
    ret

; seat_line: T1 = the spotlight's seat: its name and what it did last, tan
; if it is to move. Clobbers A, r0-r7.
seat_line:
    call lb_clear
    mov r0,#SPOT
    movx a,@r0
    mov r5,a
    call seat_base
    mov r1,#LINEBUF             ; its name, 8
    mov r4,#8
    call sl_copy
    mov a,r2                    ; its move, 7: 12 past its name
    add a,#SE_MOVE-8
    mov r2,a
    clr a
    addc a,r3
    mov r3,a
    mov r1,#LINEBUF+9
    mov r4,#7
    call sl_copy
    mov r0,#TXCOL
    mov a,#C_WHITE
    movx @r0,a
    mov r0,#ACTV
    movx a,@r0
    xrl a,r5
    jnz sl_put
    mov r0,#TXCOL
    mov a,#C_TAN
    movx @r0,a
sl_put:
    mov r0,#TXCOL               ; TXCOL2 the same
    movx a,@r0
    inc r0
    movx @r0,a
    jmp t1_say
sl_copy:                        ; r4 bytes from r3:r2 to r1, NUL a space
    call st_byte
    jnz slc_1
    mov a,#' '
slc_1:
    movx @r1,a
    inc r1
    inc r2
    mov a,r2
    jnz slc_2
    inc r3
slc_2:
    djnz r4,sl_copy
    ret

; chips: sprite 1 down the rows (fixed.inc row_write): a chip beside each
; left seat with a bet in this street (rounds 1-4) -- the server moves the
; bets into the pot as the next is dealt. Rows 0-3 and ours, 6; a right
; seat's chip is a character (page 3's row_build). Clobbers A, r0-r7.
chips:
    mov r7,#0                   ; the row
    mov r4,#(ROWS+ROW_SPR)&0FFh ; its chip's Y in ROWS
    mov r5,#Y_ROW0+CHIPDY       ; and the chip's place
cp_l:
    mov r3,#SPR_OFF
    mov a,r7                    ; its seat: 1 + 2r, us on row 6, none on the
    xrl a,#ROW_US               ; board's
    jz cp_seat
    mov a,r7
    add a,#256-ROW_BOARD
    jc cp_put
    mov a,r7
    rl a
    inc a
cp_seat:
    mov r1,a
    call our_round              ; (the showdown's pot has the bets, but the
    jz cp_put                   ; seats still show them)
    mov a,r1
    cpl a
    mov r6,a
    mov r0,#NSEAT
    movx a,@r0
    add a,r6                    ; carry: NSEAT > seat
    jnc cp_put
    mov a,r1
    call seat_base
    mov a,r2
    add a,#SE_BET
    mov r2,a
    clr a
    addc a,r3
    mov r3,a
    call st_byte
    mov r1,a
    inc r2                      ; (no seat's bet straddles a 256)
    call st_byte
    orl a,r1
    mov r3,#SPR_OFF
    jz cp_put
    mov a,r5
    mov r3,a
cp_put:
    mov r2,#ROWS>>8
    mov a,r4
    call poke_at
    mov a,r3
    call poke_b
    mov a,r4
    add a,#ROW_TAB
    mov r4,a
    mov a,r5
    add a,#ROW_PITCH
    mov r5,a
    inc r7
    mov a,r7
    xrl a,#ROW_US+1
    jnz cp_l
    ret

    if $ > 0D00h
    error "page $C has run out"
    endif

; ===========================================================================
    org 0D00h
; lr_draw: T1 = a page of lastResult from RPAGE: what fits in 16, cut at a
; space; RNEXT the next page's start (0: the first again). Clobbers A,
; r0-r7.
lr_draw:
    call lb_clear
    mov r0,#RPAGE
    movx a,@r0
    mov r2,a                    ; the byte looked at
    mov r3,#0
    mov r5,#0                   ; k: how many so far
    mov r7,#0                   ; the last space's k
lr_l:
    mov a,r2                    ; past the field: its end
    add a,#256-80
    jc lr_nul
    call st_byte
    jz lr_nul
    mov r4,a
    mov a,r5
    xrl a,#16
    jz lr_17
    mov a,r4
    xrl a,#' '
    jnz lr_n
    mov a,r5
    mov r7,a
lr_n:
    inc r5
    inc r2
    jmp lr_l
lr_nul:                         ; the end: this page is the last
    mov r4,#0
    jmp lr_put
lr_17:                          ; a 17th (r2 at it): cut at the last space
    mov a,r4
    xrl a,#' '
    jz lr_sp
    mov a,r7
    jz lr_long
    mov r5,a                    ; r7 of them; the next after the space
    mov a,r2
    add a,#256-16
    add a,r5
    inc a
    mov r4,a
    jmp lr_put
lr_sp:                          ; 16, the next after the space
    mov a,r2
    inc a
    mov r4,a
    jmp lr_put
lr_long:                        ; no space: 16, the next from the 17th
    mov a,r2
    mov r4,a
lr_put:
    mov r0,#RNEXT
    mov a,r4
    movx @r0,a
    mov r0,#RPAGE               ; r5 characters from the start, centred
    movx a,@r0
    mov r2,a
    mov a,r5
    cpl a
    add a,#17
    clr c
    rrc a
    add a,#LINEBUF
    mov r1,a
lr_c:
    mov a,r5
    jz lr_say
    call st_byte
    movx @r1,a
    inc r1
    inc r2
    dec r5
    jmp lr_c
lr_say:
    mov r0,#TXCOL
    mov a,#C_WHITE
    movx @r0,a
    inc r0
    movx @r0,a
    jmp t1_say

; t0_draw: T0 = the street on the left, then the pot and our purse "$n",
; each with its stack of chips to its left -- unless the round, the pot and
; the purse are what it shows (T0SIG). Clobbers A, r0-r7.
t0_draw:
    mov r0,#ROUND
    movx a,@r0
    mov r7,a                    ; the signature
    mov r3,#0
    mov r2,#GAME_POT
    call t0_sig
    clr a
    call seat_base
    mov a,r2
    add a,#SE_PURSE
    mov r2,a
    clr a
    addc a,r3
    mov r3,a
    call t0_sig
    mov a,r7                    ; (0FFh is "draw it": never a signature)
    inc a
    jnz t0_k
    dec r7
t0_k:
    mov r0,#T0SIG
    movx a,@r0
    xrl a,r7
    jnz t0_go
    ret
t0_go:
    mov a,r7
    movx @r0,a
    call lb_clear
    mov r0,#ROUND               ; the street
    movx a,@r0
    add a,#256-ROUND_SHOW
    jc t0_pot
    movx a,@r0
    add a,#streets&0FFh
    movp a,@a
    jz t0_pot
    call lb_str
t0_pot:
    mov r3,#0
    mov r2,#GAME_POT
    call st_byte
    mov r4,a
    inc r2
    call st_byte
    mov r5,a
    mov r0,#vdc_spr2_shape
    call stack
    mov r1,#LINEBUF+T0_POT
    call put_u16
    clr a                       ; our purse
    call seat_base
    mov a,r2
    add a,#SE_PURSE
    mov r2,a
    clr a
    addc a,r3
    mov r3,a
    call st_byte
    mov r4,a
    inc r2
    call st_byte
    mov r5,a
    mov r0,#vdc_spr3_shape
    call stack
    mov r1,#LINEBUF+T0_PURSE
    mov a,#'$'
    movx @r1,a
    inc r1
    call put_u16                ; (a fifth digit falls on TXCOL: say_line
    mov r4,#L_TOP               ; sets it after)
    jmp say_line
streets:                        ; each round's word in T0 (0: none)
    db 0, s_pre&0FFh, s_flop&0FFh, s_turn&0FFh, s_river&0FFh

    if $ > 0E00h
    error "page $D has run out"
    endif

; ===========================================================================
    org 0E00h
; moves_copy: on our turn our moves (MOVES, MAMT, MAMTH, NMOV), and at its
; start the move picked (MSEL: the first that isn't a fold). Clobbers A,
; r0-r7.
moves_copy:
    mov r3,#0                   ; (the moves are all under 256)
    mov r0,#ACTV
    movx a,@r0
    mov r4,a
    clr a                       ; our moves: none, unless it's our turn
    mov r0,#NMOV
    movx @r0,a
    mov a,r4
    jnz mv_x
    call our_round
    jz mv_x
    mov r2,#GAME_NMOVES
    call st_byte
    mov r7,a                    ; at most 5
    add a,#256-NMOVES-1
    jnc mv_n
    mov r7,#NMOVES
mv_n:
    mov a,r7
    jz mv_x
    mov r0,#NMOV
    movx @r0,a
    mov r5,#0                   ; move r5: code, amount
mv_mv:
    call move_copy
    inc r5
    mov a,r5
    xrl a,r7
    jnz mv_mv
    mov r0,#LACTV               ; our turn has just begun: a move picked
    movx a,@r0
    jz mv_keep
    mov r0,#MSEL
    clr a
    movx @r0,a
    mov r0,#MOVES               ; the first that isn't a fold
    movx a,@r0
    xrl a,#'f'
    jnz mv_x
    mov a,r7
    dec a
    jz mv_x
    mov r0,#MSEL
    mov a,#1
    movx @r0,a
    ret
mv_keep:
    mov r0,#MSEL                ; still ours: the same one, if it's still there
    movx a,@r0
    cpl a
    add a,r7                    ; carry: NMOV > MSEL
    jc mv_x
    clr a
    movx @r0,a
mv_x:
    ret

; move_copy: move r5 of the reply into MOVES (its code) and MAMT, MAMTH (the
; number in its name, BCD: "bet 10", "call 1250"; 0 for none). Clobbers A,
; r0-r4, r6.
move_copy:
    mov a,r5                    ; 99 + 13 r5
    rl a
    rl a
    mov r2,a
    rl a
    add a,r2                    ; 12 r5
    add a,r5
    add a,#GAME_MOVES
    mov r2,a
    mov a,r5
    rl a
    add a,#MOVES
    mov r1,a
    call st_byte                ; the code's two letters
    movx @r1,a
    inc r1
    inc r2
    call st_byte
    movx @r1,a
    inc r2                      ; the name (NUL-terminated): its digits
    inc r2
    mov r4,#0
    mov r1,#0
mc_l:
    call st_byte
    jz mc_x
    add a,#256-'0'
    jnc mc_n
    mov r6,a
    add a,#256-10
    jc mc_n
    mov a,r4                    ; r1:r4 = r1:r4 << 4 | digit
    swap a
    mov r4,a
    anl a,#0Fh
    xch a,r1
    swap a
    anl a,#0F0h
    orl a,r1
    mov r1,a
    mov a,r4
    anl a,#0F0h
    orl a,r6
    mov r4,a
mc_n:
    inc r2
    jmp mc_l
mc_x:
    mov a,r5
    add a,#MAMT
    mov r0,a
    mov a,r4
    movx @r0,a
    mov a,r5
    add a,#MAMTH
    mov r0,a
    mov a,r1
    movx @r0,a
    ret

    if $ > 0F00h
    error "page $E has run out"
    endif

    PAGE_TAIL
