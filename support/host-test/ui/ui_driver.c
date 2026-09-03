/* Host UI reproduction driver: replays a hand's frames through the REAL shared
 * rendering code (src/gamelogic.c) with a mock text-grid platform, printing the
 * table after each frame. Used to hunt down layout/clearing bugs without an
 * emulator.
 *
 * Build & run from repo root:
 *   cc -I support/host-test/ui -include support/host-test/ui/host_vars.h \
 *      src/gamelogic.c support/host-test/ui/mock_platform.c \
 *      support/host-test/ui/ui_driver.c -o /tmp/ui_test && /tmp/ui_test
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../../src/misc.h"
#include "../../../src/gamelogic.h"

/* ---- globals normally defined in main.c / misc.c ---- */
char serverEndpoint[50] = "";
char query[50] = "";
char playerName[12] = "you";
ClientState clientState;
char inputKey;
unsigned char prevPlayerCount, prevRound, currentCard, cardIndex, xOffset,
    fullFirst, cursorX, cursorY, waitCount, wasViewing;
signed char inputDirX, inputDirY;
uint16_t prevPot, maxJifs;
bool noAnim, doAnim, finalFlip, inputTrigger;
char tempBuffer[128];
unsigned char h, i, j, k, x, y, xx;
unsigned char playerX[8], playerY[8], moveLoc[5];
signed char playerBetX[8], playerBetY[8], playerDir[8];
char *hand, *requestedMove;
char prefs[4];

#ifdef HOST_ADAM
/* Adam 32x24 master layout (mirror of src/adam/vars.c) */
unsigned char playerXMaster[] = { 11, 0, 0, 0, 11, 30, 30, 30 };
unsigned char playerYMaster[] = { 18, 18, 9, 2, 2, 2, 9, 18 };
char playerDirMaster[] = { 1,1,1,1,1,-1,-1,-1 };
char playerBetXMaster[] = { -3, 10, 1, 10, 3, -8, -1, -8 };
char playerBetYMaster[] = { -3, -3, 4, 4, 4, 4, 4, -3 };
#elif defined(HOST_COCO3)
/* CoCo 3 master layout (mirror of src/coco/vars.c) */
unsigned char playerXMaster[] = { 17,1, 1, 1, 16, 37,37, 37 };
unsigned char playerYMaster[] = { 18, 18,10,2, 2, 2,10,18 };
char playerDirMaster[] = { 1,1,1,1,1,-1,-1,-1 };
char playerBetXMaster[] = { 1,10,8,10,3,-8,-3,-8 };
char playerBetYMaster[] = { -2, -2, 1,5,5,5,1,-2 };
#else
/* Apple II master layout (mirror of src/apple2/util.c) */
unsigned char playerXMaster[] = { 17,1, 1, 1, 15, 37,37, 37 };
unsigned char playerYMaster[] = { 18, 17, 10, 3, 3,3,10,17 };
char playerDirMaster[] = { 1,1,1,1,1,-1,-1,-1 };
char playerBetXMaster[] = { 1,10,8,10,3,-8,-3,-8 };
char playerBetYMaster[] = { -2, -2, 1,4,4,4,1,-2 };
#endif
char playerCountIndex[] = {0,4,0,0,0,0,0,0, 0,2,6,0,0,0,0,0, 0,2,4,6,0,0,0,0,
   0,2,3,5,6,0,0,0, 0,2,3,4,5,6,0,0,  0,2,3,4,5,6,7,0, 0,1,2,3,4,5,6,7};

/* ---- stubs for misc.c / screens.c pieces gamelogic references ---- */
void pause(unsigned char frames) { (void)frames; }
void clearCommonInput() { inputKey = 0; inputDirX = inputDirY = 0; inputTrigger = false; }
void readCommonInput() {}
void loadPrefs() {}
void savePrefs() {}
void showInGameMenuScreen() {}
void centerStatusText(const char *t) { drawStatusText(t); }

/* from mock_platform.c */
void dumpScreen(const char *label);
extern char screen[HEIGHT][WIDTH + 1];

unsigned char redrawGameScreen = 1; /* normally defined in screens.c */

/* Replicates showGameScreen() from src/screens.c (non-SINGLE_BUFFER path) */
static void frame(const char *label) {
#ifndef SINGLE_BUFFER_MODE
  redrawGameScreen = 1; /* apple2: full redraw every frame */
#endif

  checkIfSpectatorStatusChanged();
  checkIfPlayerCountChanged();
  animateChipsToPotOnRoundEnd();
  checkFinalFlip();

  if (redrawGameScreen) resetScreen();
  resetStateIfNewGame();

  drawPot();
  drawStreetLabel();

  if (state.playerCount > 1) {
    drawNamePurse();
    drawBets();
#ifdef SINGLE_BUFFER_MODE
    if (state.round < 5) drawCards(false);
#else
    drawCards(false);
#endif
  }

  drawGameStatus();
  drawBuffer();
  highlightActivePlayer();
#ifdef SINGLE_BUFFER_MODE
  redrawGameScreen = 0;
#endif
  prevRound = state.round;

  dumpScreen(label);
}

static void setPlayer(int n, const char *name, int status, int bet,
                      const char *move, int purse, const char *handStr) {
  strcpy(state.players[n].name, name);
  state.players[n].status = (uint8_t)status;
  state.players[n].bet = (uint16_t)bet;
  strcpy(state.players[n].move, move);
  state.players[n].purse = (uint16_t)purse;
  strcpy(state.players[n].hand, handStr);
}

static int expect(int row, int col, const char *want, const char *what) {
  if (strncmp(&screen[row][col], want, strlen(want)) != 0) {
    char got[16];
    strncpy(got, &screen[row][col], strlen(want));
    got[strlen(want)] = 0;
    printf("FAIL: %s: expected \"%s\" at (%d,%d), got \"%s\"\n", what, want, col, row, got);
    return 1;
  }
  printf("ok: %s\n", what);
  return 0;
}

int main(void) {
  int fails = 0;

  clearGameState();
  initGraphics();
  memset(&clientState, 0, sizeof(clientState));

  state.playerCount = 4;
  state.round = 1;
  state.pot = 15;
  state.activePlayer = 3;
  /* seat order: 0=YOU(bottom) 1=mid-left 2=top 3=mid-right */
  setPlayer(0, "you", 1, 0, "", 1000, "askh");
  setPlayer(1, "clyd bot", 1, 5, "post", 995, "????");
  setPlayer(2, "meg bot", 1, 10, "post", 990, "????");
  setPlayer(3, "jim bot", 1, 0, "", 1000, "????");

  frame("f1: pre-flop, blinds posted");

  /* CLYD folds, JIM calls */
  setPlayer(1, "clyd bot", 2, 5, "fold", 995, "??");
  setPlayer(3, "jim bot", 1, 10, "call", 990, "????");
  state.activePlayer = 0;
  frame("f2: clyd folded, jim called");

  /* another poll, same street (steady state) */
  frame("f3: same state, next poll");

  /* flop: bets collected, moves reset for live players, board dealt */
  state.round = 2;
  state.pot = 45;
  state.activePlayer = 1; /* seat rotation: whoever */
  state.activePlayer = 3;
  strcpy(state.community, "7dkcac");
  setPlayer(0, "you", 1, 0, "", 990, "askh");
  setPlayer(1, "clyd bot", 2, 0, "fold", 995, "??");
  setPlayer(2, "meg bot", 1, 0, "", 990, "????");
  setPlayer(3, "jim bot", 1, 0, "", 990, "????");
  frame("f4: flop dealt");

  frame("f5: flop, next poll (steady state)");

  /* YOU bet on the flop - exercises the bottom-center bet position, which on
   * 32-col platforms sits nearest the pot box. */
  setPlayer(0, "you", 1, 25, "bet", 965, "askh");
  state.activePlayer = 3;
  frame("f6: you bet 25");

#if WIDTH>=40
  /* The folded player's FOLD label must survive every frame.
   * mid-left seat: move field at x=playerX+betX=1+8=9, row=playerY+betY=11 */
  fails += expect(11, 9, "FOLD", "mid-left FOLD visible on flop");

  /* mid-right seat move field: dir<0, x=37-3-5=29..33 */
  printf("mid-right field row11: \"%.10s\"\n", &screen[11][27]);
#else
  /* 32 cols: no move text on the table; a fold shows as a single back. */
#endif

#ifdef HOST_ADAM
  /* Community board: 3 cards at 2-col pitch from (11,9); rank row is 10. */
  fails += expect(10, 11, "7DKCAC", "flop board at community row");
  /* Street label on the pot row, left of the box. */
  fails += expect(15, 3, "FLOP", "street label at (3,15)");
  fails += expect(15, 16, "45", "pot amount inside the box");
  /* YOU purse: x=11+1+6 right-justified by len(965), y=18-3+2. */
  fails += expect(17, 15, "965", "YOU purse right of center");
  /* YOU bet: left of the pot box (betX -3), clear of the box border. */
  fails += expect(16, 9, "25", "YOU bet left of pot box");
  fails += expect(16, 12, "+", "pot box corner intact");
  /* Folded mid-left seat: the second hole card's leftover columns (3-4)
   * must be blanked by drawCard's face-down clear. */
  fails += expect(9, 3, "  ", "mid-left fold residue erased");
  /* Live mid-right seat: second card's sliver at col 28 plus the first
   * card at 29-31 - both hole cards must be visible. */
  fails += expect(10, 28, "???", "right seat: both hole cards visible");
#endif

  return fails ? 1 : 0;
}
