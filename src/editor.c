/**
 * @file editor.c
 * @author Joakim Bertils
 * @version 0.1
 * @date 2021-04-18
 *
 * @brief Main editor handling.
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

#include "editor.h"

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

#define JDEDIT_TAB_STOP 4

extern void die(const char *s);

editorConfig_t E;

static void editorDrawRows(struct appendBuffer *ab);
static void editorDrawStatusBar(struct appendBuffer *ab);
static void editorDrawMessageBar(struct appendBuffer *ab);

void editorInsertRow(editorConfig_t *conf, int at, char *s, size_t len) {
  if (at < 0 || at > conf->activeBuffer->numrows) {
    return;
  }

  conf->activeBuffer->row =
      realloc(conf->activeBuffer->row,
              sizeof(erow) * (conf->activeBuffer->numrows + 1));
  memmove(&conf->activeBuffer->row[at + 1], &conf->activeBuffer->row[at],
          sizeof(erow) * (conf->activeBuffer->numrows - at));

  for (int j = at + 1; j <= conf->activeBuffer->numrows; ++j) {
    conf->activeBuffer->row[j].idx++;
  }

  conf->activeBuffer->row[at].idx = at;

  conf->activeBuffer->row[at].size = len;
  conf->activeBuffer->row[at].chars = malloc(len + 1);
  memcpy(conf->activeBuffer->row[at].chars, s, len);
  conf->activeBuffer->row[at].chars[len] = '\0';

  conf->activeBuffer->row[at].rsize = 0;
  conf->activeBuffer->row[at].render = 0;

  conf->activeBuffer->row[at].hl = NULL;

  conf->activeBuffer->row[at].hl_open_comment = 0;

  editorUpdateRow(conf, &conf->activeBuffer->row[at]);

  conf->activeBuffer->numrows++;
  conf->activeBuffer->dirty++;
}

void editorDelRow(editorConfig_t *conf, int at) {
  if (at < 0 || at >= conf->activeBuffer->numrows) {
    return;
  }

  editorFreeRow(&conf->activeBuffer->row[at]);

  memmove(&conf->activeBuffer->row[at], &conf->activeBuffer->row[at + 1],
          sizeof(erow) * (conf->activeBuffer->numrows - at - 1));

  for (int j = at; j < conf->activeBuffer->numrows - 1; ++j) {
    conf->activeBuffer->row[j].idx--;
  }

  conf->activeBuffer->numrows--;
  conf->activeBuffer->dirty++;
}

void editorInsertChar(editorConfig_t *conf, int c) {
  if (conf->activeBuffer->cy == conf->activeBuffer->numrows) {
    editorInsertRow(conf, conf->activeBuffer->numrows, "", 0);
  }

  editorRowInsertChar(conf, &conf->activeBuffer->row[conf->activeBuffer->cy],
                      conf->activeBuffer->cx, c);

  ++conf->activeBuffer->cx;
}

void editorInsertNewline(editorConfig_t *conf) {
  int leadingTabs = 0;

  if (conf->activeBuffer->cx == 0) {
    editorInsertRow(conf, conf->activeBuffer->cy, "", 0);
  } else {
    erow *row = &conf->activeBuffer->row[conf->activeBuffer->cy];
    editorInsertRow(conf, conf->activeBuffer->cy + 1,
                    &row->chars[conf->activeBuffer->cx],
                    row->size - conf->activeBuffer->cx);
    row = &conf->activeBuffer->row[conf->activeBuffer->cy];
    row->size = conf->activeBuffer->cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(conf, row);

    for (int i = 0; i < row->size; ++i) {
      if (row->chars[i] == '\t') {
        ++leadingTabs;
      } else {
        break;
      }
    }
  }

  conf->activeBuffer->cy++;
  conf->activeBuffer->cx = 0;

  for (int i = 0; i < leadingTabs; ++i) {
    editorInsertChar(conf, '\t');
  }
}

void editorDelChar(editorConfig_t *conf) {
  if (conf->activeBuffer->cy == conf->activeBuffer->numrows) {
    return;
  }

  if (conf->activeBuffer->cx == 0 && conf->activeBuffer->cy == 0) {
    return;
  }

  erow *row = &conf->activeBuffer->row[conf->activeBuffer->cy];

  if (conf->activeBuffer->cx > 0) {
    editorRowDelChar(conf, row, conf->activeBuffer->cx - 1);
    conf->activeBuffer->cx--;
  } else {
    conf->activeBuffer->cx =
        conf->activeBuffer->row[conf->activeBuffer->cy - 1].size;
    editorRowAppendString(conf,
                          &conf->activeBuffer->row[conf->activeBuffer->cy - 1],
                          row->chars, row->size);
    editorDelRow(conf, conf->activeBuffer->cy);
    conf->activeBuffer->cy--;
  }
}

void editorCreateBuffer(editorConfig_t *conf, buffer_t **buf_ptr) {
  conf->buffers =
      realloc(conf->buffers, sizeof(buffer_t *) * (conf->numBuffers + 1));

  buffer_t *new = malloc(sizeof(buffer_t));

  if (new) {
    initBuffer(new);

    new->conf = conf;

    conf->buffers[conf->numBuffers] = new;

    conf->numBuffers++;

    if (buf_ptr) {
      *buf_ptr = new;
    }
  }
}

void editorDestroyBuffer(editorConfig_t *conf, int idx) {
  if (idx < 0 || idx >= conf->numBuffers) {
    return;
  }

  buffer_t *buffer = conf->buffers[idx];

  freeBuffer(buffer);

  free(buffer);

  if (conf->numBuffers == 1) {
    free(conf->buffers);
    conf->buffers = NULL;
  } else {
    if (idx != (conf->numBuffers - 1)) {
      memmove(&conf->buffers[idx], &conf->buffers[idx + 1],
              sizeof(buffer_t *) * (conf->numBuffers - idx - 1));
    }

    buffer_t **new =
        realloc(conf->buffers, sizeof(buffer_t *) * (conf->numBuffers - 1));

    if (!new) {
      die("realloc");
    }

    conf->buffers = new;

    if ((conf->curBuffer > idx) || (idx == conf->numBuffers - 1)) {
      conf->curBuffer--;
    }

    conf->activeBuffer = conf->buffers[conf->curBuffer];
  }

  conf->numBuffers--;
}

void editorNextBuffer(editorConfig_t *conf, buffer_t **buf_ptr) {
  ++conf->curBuffer;

  if (conf->curBuffer == conf->numBuffers) {
    conf->curBuffer = 0;
  }

  conf->activeBuffer = conf->buffers[conf->curBuffer];

  if (buf_ptr) {
    *buf_ptr = conf->activeBuffer;
  }
}

void editorPrevBuffer(editorConfig_t *conf, buffer_t **buf_ptr) {
  if (conf->curBuffer == 0) {
    conf->curBuffer = conf->numBuffers;
  }

  --conf->curBuffer;

  conf->activeBuffer = conf->buffers[conf->curBuffer];

  if (buf_ptr) {
    *buf_ptr = conf->activeBuffer;
  }
}

void editorFirstBuffer(editorConfig_t *conf, buffer_t **buf_ptr) {
  conf->curBuffer = 0;

  conf->activeBuffer = conf->buffers[0];

  if (buf_ptr) {
    *buf_ptr = conf->activeBuffer;
  }
}

void editorLastBuffer(editorConfig_t *conf, buffer_t **buf_ptr) {
  conf->curBuffer = conf->numBuffers - 1;

  conf->activeBuffer = conf->buffers[conf->numBuffers - 1];

  if (buf_ptr) {
    *buf_ptr = conf->activeBuffer;
  }
}

void initBuffer(buffer_t *buffer) {
  buffer->cx = 0;
  buffer->cy = 0;
  buffer->rx = 0;

  buffer->rowoff = 0;
  buffer->coloff = 0;

  buffer->linum_width = 0;
  buffer->linum_mode = 1;

  buffer->numrows = 0;
  buffer->row = NULL;
  buffer->dirty = 0;

  buffer->filename = NULL;
  buffer->syntax = NULL;
}

void freeBuffer(buffer_t *buffer) {
  while (buffer->numrows) {
    editorDelRow(buffer->conf, buffer->numrows - 1);
  }

  free(buffer->row);

  free(buffer->filename);
}

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;

  for (j = 0; j < E.activeBuffer->numrows; ++j) {
    totlen += E.activeBuffer->row[j].size + 1;
  }

  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;

  for (j = 0; j < E.activeBuffer->numrows; ++j) {
    memcpy(p, E.activeBuffer->row[j].chars, E.activeBuffer->row[j].size);
    p += E.activeBuffer->row[j].size;
    *p = '\n';
    ++p;
  }

  return buf;
}

void editorOpen(char *filename) {
  if (E.activeBuffer->filename && E.activeBuffer->dirty) {
    char *response =
        editorPrompt("Warning! Unsaved changes. Close anyway? y/n: %s", NULL);

    if (!(strcmp(response, "y") == 0 || strcmp(response, "Y") == 0)) {
      return;
    }
  }

  E.activeBuffer->filename = strdup(filename);

  editorSelectSyntaxHighlight(&E);

  FILE *fp = fopen(filename, "r");
  if (!fp) {
    // File did not exist. Create it.
    fp = fopen(filename, "w");

    if (!fp) {
      // File could not be opened.
      die("fopen");
    }
  }

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  ssize_t bytes_read = 0;

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    bytes_read += linelen;
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
      linelen--;
    }

    editorInsertRow(&E, E.activeBuffer->numrows, line, linelen);
  }

  free(line);
  fclose(fp);
  E.activeBuffer->dirty = 0;

  editorSetStatusMessage("Opened File: %.20s - %d bytes read", filename,
                         bytes_read);
}

int editorClose() {
  while (E.numBuffers) {
    editorLastBuffer(&E, NULL);

    buffer_t *buffer = E.activeBuffer;

    if (buffer->dirty) {
      char buf[80];

      int len = snprintf(
          buf, sizeof(buf),
          "Warning! %s contains unsaved changes. Exit anyway? y/n: %%s",
          buffer->filename ? buffer->filename : "[no name]");

      char *response = editorPrompt(buf, NULL);

      if (!(strcmp(response, "y") == 0 || strcmp(response, "Y") == 0)) {
        if (response) {
          free(response);
        }
        return 1;
      }

      free(response);
    }

    editorDestroyBuffer(&E, E.numBuffers - 1);
  }

  return 0;
}

void editorSave() {
  if (E.activeBuffer->filename == NULL) {
    E.activeBuffer->filename = editorPrompt("Save as: %s", NULL);

    if (E.activeBuffer->filename == NULL) {
      editorSetStatusMessage("Save Aborted");
      return;
    }

    editorSelectSyntaxHighlight(&E);
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.activeBuffer->filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.activeBuffer->dirty = 0;
        editorSetStatusMessage("Wrote File: %.20s - %d bytes written",
                               E.activeBuffer->filename, len);
        return;
      }
    }
    close(fd);
  }

  free(buf);

  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

void editorFindCallback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl) {
    memcpy(E.activeBuffer->row[saved_hl_line].hl, saved_hl,
           E.activeBuffer->row[saved_hl_line].rsize);

    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;

    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1) {
    direction = 1;
  }

  int current = last_match;
  int i;

  for (i = 0; i < E.activeBuffer->numrows; ++i) {
    current += direction;

    if (current == -1) {
      current = E.activeBuffer->numrows - 1;
    } else if (current == E.activeBuffer->numrows) {
      current = 0;
    }

    erow *row = &E.activeBuffer->row[current];
    char *match = strstr(row->render, query);

    if (match) {
      last_match = current;
      E.activeBuffer->cy = current;
      E.activeBuffer->cx = editorRowRxToCx(row, match - row->render);
      E.activeBuffer->rowoff = E.activeBuffer->numrows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.activeBuffer->cx;
  int saved_cy = E.activeBuffer->cy;
  int saved_coloff = E.activeBuffer->coloff;
  int saved_rowoff = E.activeBuffer->rowoff;

  char *query = editorPrompt("Search: %s", editorFindCallback);

  if (query) {
    free(query);
  } else {
    E.activeBuffer->cx = saved_cx;
    E.activeBuffer->cy = saved_cy;
    E.activeBuffer->coloff = saved_coloff;
    E.activeBuffer->rowoff = saved_rowoff;
  }
}

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = terminalReadKey();

    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) {
        buf[--buflen] = '\0';
      }
    } else if (c == '\x1b') {
      editorSetStatusMessage("");

      if (callback) {
        callback(buf, c);
      }

      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");

        if (callback) {
          callback(buf, c);
        }

        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }

    if (callback) {
      callback(buf, c);
    }
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.activeBuffer->cy >= E.activeBuffer->numrows)
                  ? NULL
                  : &E.activeBuffer->row[E.activeBuffer->cy];

  switch (key) {
  case ARROW_LEFT:
    if (E.activeBuffer->cx != 0) {
      E.activeBuffer->cx--;
    } else if (E.activeBuffer->cy > 0) {
      E.activeBuffer->cy--;
      E.activeBuffer->cx = E.activeBuffer->row[E.activeBuffer->cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.activeBuffer->cx < row->size) {
      E.activeBuffer->cx++;
    } else if (row && E.activeBuffer->cx == row->size) {
      E.activeBuffer->cy++;
      E.activeBuffer->cx = 0;
    }
    break;
  case ARROW_UP:
    if (E.activeBuffer->cy != 0) {
      E.activeBuffer->cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.activeBuffer->cy < E.activeBuffer->numrows) {
      E.activeBuffer->cy++;
    }
    break;
  }

  row = (E.activeBuffer->cy >= E.activeBuffer->numrows)
            ? NULL
            : &E.activeBuffer->row[E.activeBuffer->cy];

  int rowlen = row ? row->size : 0;
  if (E.activeBuffer->cx > rowlen) {
    E.activeBuffer->cx = rowlen;
  }
}

void editorProcessKeypress() {
  int c;

  c = terminalReadKey();

  switch (c) {
  case '\r':
    editorInsertNewline(&E);
    break;
  case CTRL_KEY('q'):

    if (editorClose()) {
      return;
    }

    terminalWrite("\x1b[2J", 4);
    terminalWrite("\x1b[H", 3);

    terminalDisableRawMode(&E);

    exit(0);
    break;

  case CTRL_KEY('u'):
    editorSave();
    break;

  case CTRL_KEY('o'): {
    char *filename = editorPrompt("Open file: %s", NULL);

    if (filename) {
      editorCreateBuffer(&E, NULL);
      // TODO: Switch by name
      editorLastBuffer(&E, NULL);
      editorOpen(filename);

      free(filename);
    }
  } break;

  case CTRL_KEY('a'):
  case HOME_KEY:
    E.activeBuffer->cx = 0;
    break;
  case CTRL_KEY('e'):
  case END_KEY:
    if (E.activeBuffer->cy < E.activeBuffer->numrows) {
      E.activeBuffer->cx = E.activeBuffer->row[E.activeBuffer->cy].size;
    }
    break;

  case CTRL_KEY('s'):
    editorFind();
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (c == DEL_KEY) {
      editorMoveCursor(ARROW_RIGHT);
    }
    editorDelChar(&E);
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP) {
      E.activeBuffer->cy = E.activeBuffer->rowoff;
    } else if (c == PAGE_DOWN) {
      E.activeBuffer->cy = E.activeBuffer->rowoff + E.screenRows - 1;

      if (E.activeBuffer->cy > E.activeBuffer->numrows) {
        E.activeBuffer->cy = E.activeBuffer->numrows;
      }
    }

    int times = E.screenRows;

    while (times--) {
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
  } break;
  case CTRL_KEY('k'):
    editorDestroyBuffer(&E, E.curBuffer);
    break;
  case CTRL_KEY('r'):
    editorPrevBuffer(&E, NULL);
    break;
  case CTRL_KEY('t'):
    editorNextBuffer(&E, NULL);
    break;
  case CTRL_KEY('d'):
    editorFirstBuffer(&E, NULL);
    break;
  case CTRL_KEY('g'):
    editorLastBuffer(&E, NULL);
  case CTRL_KEY('p'):
    editorMoveCursor(ARROW_UP);
    break;
  case CTRL_KEY('n'):
    editorMoveCursor(ARROW_DOWN);
    break;
  case CTRL_KEY('b'):
    editorMoveCursor(ARROW_LEFT);
    break;
  case CTRL_KEY('f'):
    editorMoveCursor(ARROW_RIGHT);
    break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

  case CTRL_KEY('i'):
    editorInsertChar(&E, '\t');
    break;

  default:
    if (c & (~0x1F)) {
      editorInsertChar(&E, c);
    } else {
      char str[16];
      int str_len = snprintf(str, sizeof(str), "%#04x", c);
      editorSetStatusMessage("Undefined key %s", str);
    }
    break;
  }
}

//==============================================================================
// Output
//==============================================================================

void editorScroll() {
  E.activeBuffer->rx = 0;

  if (E.activeBuffer->cy < E.activeBuffer->numrows) {
    E.activeBuffer->rx = editorRowCxToRx(
        &E.activeBuffer->row[E.activeBuffer->cy], E.activeBuffer->cx);
  }

  if (E.activeBuffer->cy < E.activeBuffer->rowoff) {
    E.activeBuffer->rowoff = E.activeBuffer->cy;
  }

  if (E.activeBuffer->cy >= E.activeBuffer->rowoff + E.screenRows) {
    E.activeBuffer->rowoff = E.activeBuffer->cy - E.screenRows + 1;
  }

  if (E.activeBuffer->rx < E.activeBuffer->coloff) {
    E.activeBuffer->coloff = E.activeBuffer->rx;
  }

  if (E.activeBuffer->rx >= E.activeBuffer->coloff + E.screenCols) {
    E.activeBuffer->coloff = E.activeBuffer->rx - E.screenCols + 1;
  }
}

static void editorDrawRows(struct appendBuffer *ab) {
  int y;

  for (y = 0; y < E.screenRows; ++y) {
    int filerow = y + E.activeBuffer->rowoff;

    if (E.activeBuffer->linum_mode) {
      char buf[16];

      memset(buf, ' ', sizeof(buf));

      if (filerow < E.activeBuffer->numrows) {
        snprintf(buf, sizeof(buf), "%0*d ", E.activeBuffer->linum_width - 1,
                 filerow + 1);
      }

      abAppend(ab, buf, E.activeBuffer->linum_width);
    }

    if (filerow >= E.activeBuffer->numrows) {
      if (E.activeBuffer->numrows == 0 && y == E.screenRows / 3) {
        char welcome[80];

        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "JDEDIT editor -- version %s", "0.0.1");
        if (welcomelen > E.screenCols) {
          welcomelen = E.screenCols;
        }

        int padding = (E.screenCols - welcomelen) / 2;

        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }

        while (padding--) {
          abAppend(ab, " ", 1);
        }

        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.activeBuffer->row[filerow].rsize - E.activeBuffer->coloff;

      if (len < 0) {
        len = 0;
      }

      if (len > E.screenCols) {
        len = E.screenCols;
      }

      char *c = &E.activeBuffer->row[filerow].render[E.activeBuffer->coloff];
      unsigned char *hl =
          &E.activeBuffer->row[filerow].hl[E.activeBuffer->coloff];
      int current_color = -1;

      int j;

      for (j = 0; j < len; ++j) {
        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';

          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);

          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            abAppend(ab, buf, clen);
          }
        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          abAppend(ab, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);

          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }

          abAppend(ab, &c[j], 1);
        }
      }

      abAppend(ab, "\x1b[39m", 5);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

static void editorDrawStatusBar(struct appendBuffer *ab) {
  abAppend(ab, "\x1b[7m", 4);

  char status[80];
  char rstatus[80];

  int len = snprintf(
      status, sizeof(status), "%.20s - %d lines %s",
      E.activeBuffer->filename ? E.activeBuffer->filename : "[No Name]",
      E.activeBuffer->numrows, E.activeBuffer->dirty ? "(modified)" : "");

  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | L%d/%d | B%d/%d",
                      E.activeBuffer->syntax ? E.activeBuffer->syntax->filetype
                                             : "no ft",
                      E.activeBuffer->cy + 1, E.activeBuffer->numrows,
                      E.curBuffer + 1, E.numBuffers);

  if (len > E.screenCols + E.activeBuffer->linum_width) {
    len = E.screenCols + E.activeBuffer->linum_width;
  }

  abAppend(ab, status, len);

  while (len < E.screenCols + E.activeBuffer->linum_width) {
    if (E.screenCols + E.activeBuffer->linum_width - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      ++len;
    }
  }

  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

static void editorDrawMessageBar(struct appendBuffer *ab) {
  abAppend(ab, "\x1b[K", 3);

  int msglen = strlen(E.statusmsg);

  if (msglen > E.screenCols + E.activeBuffer->linum_width) {
    msglen = E.screenCols + E.activeBuffer->linum_width;
  }

  if (msglen && time(NULL) - E.statusmsg_time < 5) {
    abAppend(ab, E.statusmsg, msglen);
  }
}

void editorRefreshScreen() {
  if (terminalGetWindowSize(&E.windowRows, &E.windowCols) == -1) {
    die("getWindowSize");
  }

  E.screenRows = E.windowRows - 2;
  E.screenCols = E.windowCols - E.activeBuffer->linum_width;

  if (E.activeBuffer->linum_mode) {
    char buf[16];

    snprintf(buf, sizeof(buf), "%d ", E.activeBuffer->numrows);

    E.activeBuffer->linum_width = strlen(buf);

    E.screenCols = E.windowCols - E.activeBuffer->linum_width;
  }

  editorScroll();

  struct appendBuffer ab;

  abInit(&ab);

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
           (E.activeBuffer->cy - E.activeBuffer->rowoff) + 1,
           (E.activeBuffer->rx - E.activeBuffer->coloff) + 1 +
               E.activeBuffer->linum_width);

  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  terminalWrite(ab.b, ab.len);

  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);

  va_end(ap);

  E.statusmsg_time = time(NULL);
}

//==============================================================================
// Init
//==============================================================================

void editorInit() {
  // Make sure buffers is malloc'ed, otherwise realloc fails
  E.buffers = malloc(sizeof(buffer_t *));

  editorCreateBuffer(&E, NULL);

  E.curBuffer = 0;

  E.activeBuffer = E.buffers[E.curBuffer];

  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;

  if (terminalGetWindowSize(&E.windowRows, &E.windowCols) == -1) {
    die("getWindowSize");
  }

  E.screenRows = E.windowRows - 2;
  E.screenCols = E.windowCols - E.activeBuffer->linum_width;
}
