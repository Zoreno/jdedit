/**
 * @file editor.h
 * @author Joakim Bertils
 * @version 0.1
 * @date 2021-04-18
 *
 * @brief Editor interface.
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

#ifndef _EDITOR_H
#define _EDITOR_H

#include "row.h"
#include "syntax.h"

#include <termios.h>
#include <time.h>

#define JDEDIT_TAB_STOP 4

struct editorConfig;

typedef struct buffer {
  int cx;
  int cy;
  int rx;
  int rowoff;
  int coloff;
  int linum_width;
  int linum_mode;
  int numrows;
  erow *row;
  int dirty;
  char *filename;
  struct editorSyntax *syntax;
  struct editorConfig *conf;
} buffer_t;

typedef struct editorConfig {
  buffer_t *activeBuffer;

  buffer_t **buffers;
  int numBuffers;

  int curBuffer;

  int windowRows;
  int windowCols;

  int screenRows;
  int screenCols;

  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
} editorConfig_t;

void editorInsertRow(editorConfig_t *conf, int at, char *s, size_t len);
void editorDelRow(editorConfig_t *conf, int at);
void editorInsertChar(editorConfig_t *conf, int c);
void editorInsertNewline(editorConfig_t *conf);
void editorDelChar(editorConfig_t *conf);

void editorCreateBuffer(editorConfig_t *conf, buffer_t **buf_ptr);
void editorDestroyBuffer(editorConfig_t *conf, int idx);

/*
void editorSwitchBufferByName(editorConfig_t *conf,
                              const char *name,
                              buffer_t **buf_ptr);
*/

void editorNextBuffer(editorConfig_t *conf, buffer_t **buf_ptr);
void editorPrevBuffer(editorConfig_t *conf, buffer_t **buf_ptr);
void editorFirstBuffer(editorConfig_t *conf, buffer_t **buf_ptr);
void editorLastBuffer(editorConfig_t *conf, buffer_t **buf_ptr);

void initBuffer(buffer_t *buffer);
void freeBuffer(buffer_t *buffer);

char *editorRowsToString(int *buflen);
void editorOpen(char *filename);
int editorClose();
void editorSave();
void editorFindCallback(char *query, int key);
void editorFind();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorMoveCursor(int key);
void editorProcessKeypress();
void editorScroll();
void editorRefreshScreen();
void editorSetStatusMessage(const char *fmt, ...);
void editorInit();

#endif
