/* -----------------------------------------------------------------------
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 *  pspdebugkb.c - Simple screen debug keyboard
 *
 * Copyright (c) 2006 Mike Mallett <mike@nerdcore.net>
 *
 * $Id: pspdebugkb.c 2112 2006-12-22 10:53:20Z tyranid $ 
 * -----------------------------------------------------------------------
 */
 
#include <pspdebug.h>
#include <pspctrl.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

extern MenuState menu;
extern int resume_count;

 /** FG and BG colour of unhighlighted characters */
unsigned int PSP_DEBUG_KB_CHAR_COLOUR = 0xFFBBBBBB;
unsigned int PSP_DEBUG_KB_BACK_COLOUR = 0xFF000000;
  /** FG and BG colour of highlighted character */
unsigned int PSP_DEBUG_KB_CHAR_HIGHLIGHT = 0xFF0000FF;
unsigned int PSP_DEBUG_KB_BACK_HIGHLIGHT = 0xFF000000;

extern int snprintf(char *str, size_t str_m, const char *fmt, /*args*/ ...);

static char loCharTable[PSP_DEBUG_KB_NUM_ROWS][PSP_DEBUG_KB_NUM_CHARS] = {
  { '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=' },
  { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\\' }, // { } remember to remove the brackets
  { 0x20, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0x20 },
  { 0x20, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0x20, 0x20 }
};

 //this is the row for the {}
static char hiCharTable[PSP_DEBUG_KB_NUM_ROWS][PSP_DEBUG_KB_NUM_CHARS] = {
  { '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+' },
  { 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '|' },
  { 0x20, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', 0x20 },
  { 0x20, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0x20, 0x20 }
};

static char *commandRow[] = { "Shift", "[    ]", "Back", "Clear", "Done" };

char charTable[PSP_DEBUG_KB_NUM_ROWS][PSP_DEBUG_KB_NUM_CHARS];

/* Switch charTable when Shift is pressed */
void pspDebugKbShift(int* shiftState) {
  int i, j;
  if (*shiftState != 0) {
    for (i=0; i<PSP_DEBUG_KB_NUM_ROWS; i++) {
      for (j=0; j<PSP_DEBUG_KB_NUM_CHARS; j++) {
		charTable[i][j] = hiCharTable[i][j];
      }
    }
    *shiftState = 0;
  } else {
    for (i=0; i<PSP_DEBUG_KB_NUM_ROWS; i++) {
      for (j=0; j<PSP_DEBUG_KB_NUM_CHARS; j++) {
      	charTable[i][j] = loCharTable[i][j];
      }
    }
    *shiftState = 1;
  }
  pspDebugKbDrawBox();
}

void pspDebugKbDrawKey(int row, int col, int highlight) {
  int i;
  int spacing = 0;
  int charsTo = 0;
  int charsTotal = 0;

  if (highlight) {
    pspDebugScreenSetTextColor(PSP_DEBUG_KB_CHAR_HIGHLIGHT);
    pspDebugScreenSetBackColor(PSP_DEBUG_KB_BACK_HIGHLIGHT);
  } else {
    pspDebugScreenSetTextColor(PSP_DEBUG_KB_CHAR_COLOUR);
    pspDebugScreenSetBackColor(PSP_DEBUG_KB_BACK_COLOUR);
  }

  if (row == PSP_DEBUG_KB_COMMAND_ROW) {
    for (i=0; i<PSP_DEBUG_KB_NUM_COMMANDS; i++) {
      charsTotal += strlen(commandRow[i]);
      if (i < col) { charsTo += strlen(commandRow[i]); }
    }
    spacing = (PSP_DEBUG_KB_BOX_WIDTH - charsTotal) / (PSP_DEBUG_KB_NUM_COMMANDS + 1);
    pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + (spacing * (col + 1)) + charsTo, PSP_DEBUG_KB_BOX_Y + (PSP_DEBUG_KB_SPACING_Y * (row + 2)));
    puts(commandRow[col]);
  } else {
    pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + PSP_DEBUG_KB_OFFSET_X + (PSP_DEBUG_KB_SPACING_X * col), PSP_DEBUG_KB_BOX_Y + (PSP_DEBUG_KB_SPACING_Y * (row + 2)));
    if (charTable[row][col] == 0) {
      puts(" ");
    } else {
    printf("%c", charTable[row][col]);
  }
  }
}

void pspDebugKbClearBox() {
  int i, j;

  pspDebugScreenSetTextColor(PSP_DEBUG_KB_CHAR_COLOUR);
  pspDebugScreenSetBackColor(PSP_DEBUG_KB_BACK_COLOUR);

  for (i = PSP_DEBUG_KB_BOX_X; i <= PSP_DEBUG_KB_BOX_X + PSP_DEBUG_KB_BOX_WIDTH; i++) {
    for (j = PSP_DEBUG_KB_BOX_Y; j <= PSP_DEBUG_KB_BOX_Y + PSP_DEBUG_KB_BOX_HEIGHT; j++) {
      pspDebugScreenSetXY(i, j);
      puts(" ");
    }
  }
}

void pspDebugKbDrawBox() {
  int i, j;

  pspDebugScreenSetTextColor(PSP_DEBUG_KB_CHAR_COLOUR);
  pspDebugScreenSetBackColor(PSP_DEBUG_KB_BACK_COLOUR);

  pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X, PSP_DEBUG_KB_BOX_Y);
  puts("+");
  pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X, PSP_DEBUG_KB_BOX_Y + PSP_DEBUG_KB_BOX_HEIGHT);
  puts("+");
  pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + PSP_DEBUG_KB_BOX_WIDTH, PSP_DEBUG_KB_BOX_Y);
  puts("+");
  pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + PSP_DEBUG_KB_BOX_WIDTH, PSP_DEBUG_KB_BOX_Y + PSP_DEBUG_KB_BOX_HEIGHT);
  puts("+");

  for (i = 1; i < PSP_DEBUG_KB_BOX_WIDTH; i++) {
    pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + i, PSP_DEBUG_KB_BOX_Y);
    puts("-");
    pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + i, PSP_DEBUG_KB_BOX_Y + PSP_DEBUG_KB_BOX_HEIGHT);
    puts("-");
  }

  for (i = 1; i < PSP_DEBUG_KB_BOX_HEIGHT; i++) {
    pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X, PSP_DEBUG_KB_BOX_Y + i);
    puts("|");
    pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + PSP_DEBUG_KB_BOX_WIDTH, PSP_DEBUG_KB_BOX_Y + i);
    puts("|");
  }

  for (i = 0; i < PSP_DEBUG_KB_NUM_ROWS; i++) {
    for (j = 0; j < PSP_DEBUG_KB_NUM_CHARS; j++) {
      pspDebugKbDrawKey(i, j, 0);
    }
  }

  for (i = 0; i < PSP_DEBUG_KB_NUM_COMMANDS; i++) {
    pspDebugKbDrawKey(PSP_DEBUG_KB_COMMAND_ROW, i, 0);
  }
}

void pspDebugKbDrawString(char* str) {
  int i;

  pspDebugScreenSetTextColor(PSP_DEBUG_KB_CHAR_COLOUR);
  pspDebugScreenSetBackColor(PSP_DEBUG_KB_BACK_COLOUR);

  pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + ((PSP_DEBUG_KB_BOX_WIDTH - PSP_DEBUG_KB_MAXLEN) / 2), PSP_DEBUG_KB_BOX_Y + 1);
  for (i=0; i<PSP_DEBUG_KB_MAXLEN; i++) {
    puts("_");
  }

  pspDebugScreenSetXY(PSP_DEBUG_KB_BOX_X + ((PSP_DEBUG_KB_BOX_WIDTH - PSP_DEBUG_KB_MAXLEN) / 2), PSP_DEBUG_KB_BOX_Y + 1);
  puts(str);
}

void pspDebugKbInit(char* str, int len) {
	// set the colors
	extern Colors colors;

	PSP_DEBUG_KB_CHAR_COLOUR = colors.color02;
	PSP_DEBUG_KB_BACK_COLOUR = colors.bgcolor;
	PSP_DEBUG_KB_CHAR_HIGHLIGHT = colors.color01;
	PSP_DEBUG_KB_BACK_HIGHLIGHT = colors.bgcolor;

	int row = PSP_DEBUG_KB_COMMAND_ROW;
	int col = PSP_DEBUG_KB_NUM_COMMANDS - 1;
	int shifted = 1;
	int inputDelay = 200000;

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	SceCtrlData input, lastinput;
	sceCtrlPeekBufferPositive(&input, 1);
	sceCtrlPeekBufferPositive(&lastinput, 1);

	pspDebugKbClearBox();

	// Initialize charTable
	pspDebugKbDrawBox();
	pspDebugKbShift(&shifted);
	pspDebugKbDrawKey(row, col, 1);
	pspDebugKbDrawString(str);

	while(1) {
		sceCtrlPeekBufferPositive(&input, 1);

		// make sure home button hasn't messed up menu
		if(input.Buttons & PSP_CTRL_HOME || resume_count != scePowerGetResumeCount()) {
			menu.visible = 0;
			resume_count = scePowerGetResumeCount();
			return;
		}

		if(input.Buttons != lastinput.Buttons || input.TimeStamp >= lastinput.TimeStamp + inputDelay) {
			if(input.Buttons & PSP_CTRL_LEFT && col > 0 && (row == PSP_DEBUG_KB_COMMAND_ROW || charTable[row][col - 1])) {
				// Unhighlight the old character
				pspDebugKbDrawKey(row, col, 0);

				// Print the new character highlighted
				pspDebugKbDrawKey(row, --col, 1);
			}

			if(input.Buttons & PSP_CTRL_RIGHT && ((row == PSP_DEBUG_KB_COMMAND_ROW && col < PSP_DEBUG_KB_NUM_COMMANDS - 1)
				|| (row != PSP_DEBUG_KB_COMMAND_ROW && col < PSP_DEBUG_KB_NUM_CHARS - 1 && charTable[row][col + 1]))) {
				pspDebugKbDrawKey(row, col, 0);
				pspDebugKbDrawKey(row, ++col, 1);
			}

			if(input.Buttons & PSP_CTRL_UP && row > 0) {
				if(row == PSP_DEBUG_KB_COMMAND_ROW) {
					pspDebugKbDrawKey(row, col, 0);
					if(col == PSP_DEBUG_KB_NUM_COMMANDS - 1) {
						col = PSP_DEBUG_KB_NUM_CHARS - 1;
					} else {
						col = (col * (PSP_DEBUG_KB_NUM_CHARS - 1)) / (PSP_DEBUG_KB_NUM_COMMANDS - 1);
					}
					do {
						row--;
					} while (charTable[row][col] == 0 && row > 0);
					pspDebugKbDrawKey(row, col, 1);
				} else if (charTable[row - 1][col]) {
					pspDebugKbDrawKey(row, col, 0);
					pspDebugKbDrawKey(--row, col, 1);
				}
			}

			if(input.Buttons & PSP_CTRL_DOWN && row != PSP_DEBUG_KB_COMMAND_ROW) {
				pspDebugKbDrawKey(row, col, 0);
				do {
					row++;
				} while (charTable[row][col] == 0 &&
				row != PSP_DEBUG_KB_COMMAND_ROW);
				if (row == PSP_DEBUG_KB_COMMAND_ROW) {
					col = (col * (PSP_DEBUG_KB_NUM_COMMANDS - 1)) / (PSP_DEBUG_KB_NUM_CHARS - 1);
				}
				pspDebugKbDrawKey(row, col, 1);
			}

			if(input.Buttons & PSP_CTRL_SELECT) {
				pspDebugKbShift(&shifted);
				pspDebugKbDrawKey(row, col, 1);
			}

			if(input.Buttons & PSP_CTRL_START) {
				return;
			}

			if(input.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
				if(row == PSP_DEBUG_KB_COMMAND_ROW) {
					switch(col) {
						case 0: // Shift
							pspDebugKbShift(&shifted);
							pspDebugKbDrawKey(row, col, 1);
							break;
						case 1: // Space
							if(strlen(str) + 1 < len) {
								snprintf(str, strlen(str) + 2, "%s ", str);
								pspDebugKbDrawString(str);
							}
							break;
						case 2: // Back
							if(strlen(str) > 0) {
								str[strlen(str) - 1] = 0;
								pspDebugKbDrawString(str);
							}
							break;
						case 3: // Clear
							memset(str, 0, len);
							pspDebugKbDrawString(str);
							break;
						case 4: // Done
							return;
					};
				} else {
					if(strlen(str) + 1 < len) {
						str[strlen(str)] = charTable[row][col];
						pspDebugKbDrawString(str);
					}
				}
			}

			if(input.Buttons & (PSP_CTRL_TRIANGLE | PSP_CTRL_SQUARE)) {
				if(strlen(str) > 0) {
					str[strlen(str)-1] = 0;
					pspDebugKbDrawString(str);
				}
			}

			lastinput = input;
		}
	}
}
