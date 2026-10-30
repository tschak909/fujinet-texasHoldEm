; p3.inc -- page 3: the table, when a state has come. Everything the ticks
; need is taken here (the reply window is repainted once the next request
; is under way): the round, who is to move and their clock, and on our turn
; our moves. Then the rows whose seat changed (a signature a row: a whole
; table is eight rows and frames of work). Page 6 does the rest of the
; state, in the same pass of the main loop: T0, T1, the sounds.
;
; Rows 0-3 are the opponents, two to a row: seats 1+2r on the left, 2+2r on
; the right. Rows 4-5 are the board, rows 6-7 us, seat 0 (the server puts us
; first).

    PAGE_HEAD
    db e_state&0FFh, e_title&0FFh
e_state:
    jmp on_state
e_title:
    jmp title

on_state:
    call hdr_copy
    mov r0,#LROUND              ; the table's screen, afresh
    movx a,@r0
    inc a
    jnz os_up
    call scr_table
    call rsig_clear
os_up:
    call spot_mark              ; the spotlight's marker leads the rows
    jmp rows_draw

; hdr_copy: the header's fields into XRAM; the clock's second begun; and the
; spotlight on whoever is to move, when that changes (our moves are page
; 6's: moves_copy). Clobbers A, r0-r7.
hdr_copy:
    mov r3,#0
    mov r5,#hdr_tab&0FFh
hc_l:
    mov a,r5
    movp a,@a                   ; the field's offset (0: done)
    jz hc_moves
    mov r2,a
    call st_byte
    mov r4,a
    inc r5
    mov a,r5
    movp a,@a                   ; where it goes
    mov r0,a
    mov a,r4
    movx @r0,a
    inc r5
    jmp hc_l
hc_moves:
    mov r0,#SECF
    mov a,#60
    movx @r0,a
    mov r0,#ACTV                ; the spotlight follows the turn
    movx a,@r0
    mov r4,a
    mov r0,#LACTV
    movx a,@r0
    xrl a,r4
    jz hc_spot
    mov a,r4
    add a,#256-MAXSEATS
    jc hc_spot
    mov a,r4
    mov r0,#SPOT
    movx @r0,a
hc_spot:
    ret

hdr_tab:                        ; (offset, where)
    db GAME_ROUND, ROUND
    db GAME_ACTIVE, ACTV
    db GAME_MOVETIME, MOVET
    db GAME_COUNT, NSEAT
    db 0

; title: the boot's title, on the table: a royal flush on the board, aces in
; our hand, two hands face down across the felt, the name above and below.
; Clobbers A, r0-r7.
title:
    call scr_table
    mov r7,#tt_flush&0FFh       ; (row, 10 card bytes) x 5
tt_l:
    mov a,r7
    movp a,@a
    inc a
    jz tt_lines
    dec a
    mov r0,#ROWN
    movx @r0,a
    inc r7
    mov r0,#CARDS
    mov r6,#2*NCARDS
tt_c:
    mov a,r7
    movp a,@a
    movx @r0,a
    inc r0
    inc r7
    djnz r6,tt_c
    mov a,#' '                  ; no name
    movx @r0,a
    mov a,r7
    mov r0,#TMP2
    movx @r0,a
    call row_put
    mov r0,#TMP2
    movx a,@r0
    mov r7,a
    jmp tt_l
tt_lines:
    call grid_flush
    mov r4,#L_TOP
    mov a,#s_title&0FFh
    call say
    mov r4,#L_BOT
    mov a,#s_fuji&0FFh
    jmp say
tt_flush:
    db 1, "????  ????"
    db 4, "thjhqhkhah"
    db 5, "thjhqhkhah"
    db 6, "  asac    "
    db 7, "  asac    "
    db 0FFh

    if $ > 900h
    error "page $8 has run out"
    endif

; ===========================================================================
    org 900h
    include "disp.inc"

; ===========================================================================
    org 0A00h
    include "text.inc"

s_title: db "  TEXAS HOLDEM",0
s_fuji:  db "FUJINET ODYSSEY2",0


; ===========================================================================
    org 0B00h
    include "screen.inc"
    include "rx.inc"

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

    if $ > 0C00h
    error "page $B has run out"
    endif

; ===========================================================================
    org 0C00h
    include "table.inc"

; rows_draw: every row whose seat has changed since it was drawn; ROWSNEW
; if any has. Clobbers A, r0-r7.
rows_draw:
    clr a
    mov r0,#ROWSNEW
    movx @r0,a
rd_l:
    mov r0,#ROWN
    movx @r0,a
    call row_build              ; CARDS, ROWNAME, ROWNC; A = the signature
    mov r4,a
    inc a                       ; (0FFh is "draw it": never a signature)
    jnz rd_k
    dec r4
rd_k:
    mov r0,#ROWN
    movx a,@r0
    add a,#RSIG
    mov r0,a
    movx a,@r0
    xrl a,r4
    jz rd_n
    mov a,r4
    movx @r0,a
    mov r0,#ROWSNEW
    mov a,#1
    movx @r0,a
    call grid_add               ; its new cards' faces first, then the
    call grid_flush             ; characters on them, then the faces of
    call row_put                ; any gone: neither order ever shows
    call grid_flush             ; characters on bare felt
rd_n:
    mov r0,#ROWN
    movx a,@r0
    inc a
    mov r6,a
    xrl a,#NROWS
    jz rd_g
    mov a,r6
    jmp rd_l
rd_g:
    ret

    include "sprites.inc"

    if $ > 0E00h
    error "page $D has run out"
    endif

; ===========================================================================
    org 0E00h

; rsig_clear: every row redrawn with the next state, and T0 and T1 (page
; 6). Clobbers A, r0, r1.
rsig_clear:
    mov r0,#RSIG
    mov r1,#NROWS
    mov a,#0FFh
rs_l:
    movx @r0,a
    inc r0
    djnz r1,rs_l
    mov r0,#T0SIG
    movx @r0,a
    mov r0,#T1SIG
    movx @r0,a
    ret

; row_build: row ROWN into CARDS, ROWNAME, ROWNC (table.inc row_put); A = a
; signature of it all. Rows 0-3: seat 1+2r on the left (ROWNAME, ROWNC,
; cards 0-1), 2+2r on the right (its initial at CARDS+5, its colour and chip
; in ROWNAME+1, cards 3-4). Rows 4-5: the board. Rows 6-7: us, cards 1-2,
; our initial on row 6. Clobbers A, r0-r7.
row_build:
    mov r0,#CARDS               ; nothing yet (ROWNAME follows CARDS)
    mov r1,#2*NCARDS+1
    mov a,#' '
rb_c:
    movx @r0,a
    inc r0
    djnz r1,rb_c
    clr a
    movx @r0,a
    mov r7,a                    ; the signature
    mov r0,#ROWN
    movx a,@r0
    mov r4,a
    add a,#256-ROW_BOARD
    jnc rb_opp
    add a,#256-(ROW_US-ROW_BOARD)
    jc rb_us
    mov r2,#0                   ; 4, 5: the board
    call rx_winr
    mov r2,#GAME_COMMUNITY
    mov r1,#CARDS
    mov r5,#2*NCARDS
    call rb_copy
    mov a,r7
    ret
rb_us:
    mov r5,#0
    mov r1,#CARDS+2
    call seat_in
    mov r5,a
    mov r0,#ROWN
    movx a,@r0
    xrl a,#ROW_US+1
    jnz rb_left
    mov r5,#' '                 ; (no initial on the hand's bottom)
    jmp rb_left
rb_opp:
    mov a,r4                    ; the right seat: 2 + 2r
    rl a
    add a,#2
    mov r5,a
    mov r1,#CARDS+6
    call seat_in
    mov r0,#CARDS+5
    movx @r0,a
    mov r0,#ROWNAME+1
    mov a,r4
    movx @r0,a
    mov r0,#ROWN                ; the left: 1 + 2r
    movx a,@r0
    rl a
    inc a
    mov r5,a
    mov r1,#CARDS
    call seat_in
    mov r5,a
rb_left:
    mov r0,#ROWNAME
    mov a,r5
    movx @r0,a
    mov r0,#ROWNC               ; (its chip is a sprite: page 6's chips)
    mov a,r4
    anl a,#7
    movx @r0,a
    mov a,r7
    ret

; seat_in: seat r5: its cards (4 bytes) to r1 on if it's in the hand; A = its
; initial (a space: no such seat), r4 its colour (tan to move, grey out of
; the hand, cyan us, else white) and bit 7 its chip (a bet this street),
; both added to the signature r7. The record is read through one window
; (rx_winr): 33 bytes always fit. Clobbers r0-r3, r5, r6.
seat_in:
    mov a,r5                    ; a seat past the table's: none
    cpl a
    mov r4,a
    mov r0,#NSEAT
    movx a,@r0
    add a,r4                    ; carry: NSEAT > seat
    mov r4,#C_DGREY
    mov a,#' '
    jnc si_x
    mov a,r5
    call seat_base              ; its record, r3:r2: the window r3:r2 / 16,
    mov a,r2                    ; the record at r3 in it
    anl a,#0Fh
    xch a,r2
    anl a,#0F0h
    orl a,r3
    swap a
    xch a,r2
    mov r3,a
    call rx_winr
    mov a,r3                    ; its initial
    call rx_get
    mov r0,#SCR0                ; (card_codes's scratch, free till row_put)
    movx @r0,a
    mov a,r3                    ; its status
    add a,#SE_STATUS
    call rx_get
    xrl a,#PS_PLAYING
    mov r0,#TMP2
    movx @r0,a                  ; 0: in the hand
    mov r4,#C_TAN               ; to move
    mov r0,#ACTV
    movx a,@r0
    xrl a,r5
    jz si_col
    mov r4,#C_DGREY             ; out of the hand
    mov r0,#TMP2
    movx a,@r0
    jnz si_col
    mov r4,#C_CYAN              ; us (the spotlight is a sprite's)
    mov a,r5
    jz si_col
    mov r4,#C_WHITE
si_col:
    mov r0,#ROUND               ; its chip: a bet in rounds 1-4 (the
    movx a,@r0                  ; showdown's pot has the bets, but the
    dec a                       ; seats still show them)
    add a,#256-4
    jc si_hand
    mov a,r3                    ; its bet
    add a,#SE_BET
    call rx_get
    mov r5,a
    mov a,r3
    add a,#SE_BET+1
    call rx_get
    orl a,r5
    jz si_hand
    mov a,r4
    orl a,#80h
    mov r4,a
si_hand:
    mov r0,#TMP2
    movx a,@r0
    jnz si_n
    mov a,r3                    ; its cards
    add a,#SE_HAND
    mov r2,a
    mov r5,#4
    call rb_copy
si_n:
    mov r0,#SCR0
    movx a,@r0
si_x:
    mov r6,a                    ; the signature: rotate, add colour, initial
    mov a,r4
    swap a
    add a,r6
    xch a,r7
    rl a
    add a,r7
    mov r7,a
    mov a,r6
    ret

; rb_copy: r5 bytes of the reply's window (rx_winr) from offset r2 to r1
; on, each added to the signature r7 (rotate, add). Clobbers A; r1, r2
; moved on, r5 0.
rb_copy:
    mov a,r2
    call rx_get
    movx @r1,a
    xch a,r7
    rl a
    add a,r7
    mov r7,a
    inc r1
    inc r2
    djnz r5,rb_copy
    ret

    if $ > 0F00h
    error "page $E has run out"
    endif

    PAGE_TAIL
