; tablebgt.inc -- the table screen's row backgrounds, in ONE place.
;
; Every bank supplies its OWN CDBGT, because a bank switch replaces every byte
; of $1000-$17FF and each bank is a different screen: the lobby's list wants no
; bands and the table's eight two-row seats need them to separate at all. So
; the table needs this table three times over -- cdgame draws it, cdcomp
; composes into it, and cdnet draws it too, because NFRAME keeps the picture up
; through a /state or a /move rather than blanking the screen for the round
; trip.
;
; THREE COPIES IS THE ARCHITECTURE. THREE SOURCES OF TRUTH WAS A BUG. 5 Card
; Stud kept them in step by a comment saying to, and when this port moved the
; seats to row 0 and the board to 16-17, cdgame and cdcomp were edited and
; cdnet was not. The symptom is worth recording because it is so easy to
; misread: the table looked right, and every poll the grey bands jumped by one
; row for the length of the transaction -- a shimmer once every ninety frames
; that reads as a display timing fault rather than as a stale table.
;
; It is an include now. There is nothing left to keep in step.
;
; TWENTY-TWO ENTRIES, not twenty-one: the seam line at the bottom of row 20
; programmes row 21, which does not exist, and making that entry felt is what
; turns the handover to the bottom band into a colour the kernel was going to
; write anyway.
;
; Seats alternate black and near-black in PAIRS. Eight two-row seats with no
; gap between them run together otherwise, and there is no row to spare for a
; rule.
CDBGT:  DB      CBLACK,CBLACK                   ; 0-1   seat 0 -- you
        DB      CDARK,CDARK                     ; 2-3   seat 1
        DB      CBLACK,CBLACK                   ; 4-5   seat 2
        DB      CDARK,CDARK                     ; 6-7   seat 3
        DB      CBLACK,CBLACK                   ; 8-9   seat 4
        DB      CDARK,CDARK                     ; 10-11 seat 5
        DB      CBLACK,CBLACK                   ; 12-13 seat 6
        DB      CDARK,CDARK                     ; 14-15 seat 7
        DB      CBLACK,CBLACK                   ; 16-17 the board, the pot and
                                                ;       your purse
        DB      CBLACK,CBLACK,CBLACK            ; 18-20 the bottom bar
        DB      CGREEN                          ; 21    back to the felt
