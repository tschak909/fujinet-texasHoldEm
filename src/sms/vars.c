/**
 * @brief   Global variables for the Sega Master System
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details
 */

#ifdef BUILD_SMS

// 32x24 seat layout, the Hold'em NES's: the ColecoVision's (which keeps the
// middle clear for the community cards) with the bottom and side seats pulled
// up by one. Row 22 is the felt's rounded corners (resetScreen) and the upper
// line of a two-line status, so the bottom cards stop at row 21, and the side
// seats come up with them so a bottom seat's purse clears the side seat's
// cards. Hold'em never draws a greyed hidden hole card, so seat 0 need not sit
// on an even column the way the 5 Card Stud layout's does.
unsigned char playerXMaster[] = { 17, 1, 1, 1, 15, 29, 29, 29 };
unsigned char playerYMaster[] = { 17, 17, 10, 3, 2, 3, 10, 17 };

char playerDirMaster[] = { 1, 1, 1, 1, 1, -1, -1, -1 };
char playerBetXMaster[] = { 1, 10, 10, 10, 3, -8, -8, -8 };
char playerBetYMaster[] = { -2, -1, 2, 4, 5, 4, 2, -1 };

//                               2                3                4
char playerCountIndex[] = {0,4,0,0,0,0,0,0, 0,2,6,0,0,0,0,0, 0,2,4,6,0,0,0,0,
// 5                6                 7                8
    0,2,3,5,6,0,0,0, 0,2,3,4,5,6,0,0,  0,2,3,4,5,6,7,0, 0,1,2,3,4,5,6,7};

#endif /* BUILD_SMS */
