/**
 * @file main.c
 * @author Joakim Bertils
 * @version 0.1
 * @date 2021-04-18
 *
 * @brief Program entry point
 *
 * @copyright Copyright (C) 2021, Joakim Bertils
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https: //www.gnu.org/licenses/>.
 *
 */

//==============================================================================
// Includes
//==============================================================================

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

// TODO: All these files does not need to be included right?
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "append_buffer.h"
#include "editor.h"
#include "key.h"
#include "row.h"
#include "syntax.h"
#include "terminal.h"

//==============================================================================
// Defines
//==============================================================================

// TODO: Remove the need for this extern.
extern editorConfig_t E;

//==============================================================================
// Exit
//==============================================================================

void die(const char *s) {
  terminalWrite("\x1b[2J", 4);
  terminalWrite("\x1b[H", 3);

  terminalDisableRawMode(&E);

  perror(s);
  exit(1);
}

//==============================================================================
// Main
//==============================================================================

int main(int argc, char **argv) {
  terminalEnableRawMode(&E);
  editorInit();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    if (E.numBuffers == 0) {
      editorCreateBuffer(&E, NULL);
    }

    editorRefreshScreen();
    editorProcessKeypress();
  }

  terminalDisableRawMode(&E);

  return 0;
}

//==============================================================================
// End of file
//==============================================================================
