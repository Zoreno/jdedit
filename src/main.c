/**
 * @file main.c
 */

//==============================================================================
// Includes
//==============================================================================

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../config.h"

#include "syntax.h"
#include "editor.h"
#include "row.h"
#include "terminal.h"
#include "key.h"
#include "append_buffer.h"

//==============================================================================
// Defines
//==============================================================================

#define JDEDIT_TAB_STOP 4


//==============================================================================
// Data
//==============================================================================

struct editorConfig E;

//==============================================================================
// Filetypes
//==============================================================================

//==============================================================================
// Prototypes
//==============================================================================

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorClose();

//==============================================================================
// Exit
//==============================================================================

void die(const char *s)
{
    terminalWrite("\x1b[2J", 4);
    terminalWrite("\x1b[H", 3);

    terminalDisableRawMode(&E);

    perror(s);
    exit(1);
}

//==============================================================================
// Row Operations
//==============================================================================

int editorRowCxToRx(erow *row, int cx)
{
    int rx = 0;
    int j;

    for(j = 0; j < cx; ++j)
    {
        if(row->chars[j] == '\t')
        {
            rx += (JDEDIT_TAB_STOP - 1) - (rx % JDEDIT_TAB_STOP);
        }

        ++rx;
    }

    return rx;
}

int editorRowRxToCx(erow *row, int rx)
{
    int cur_rx = 0;
    int cx;

    for(cx = 0; cx < row->size; ++cx)
    {
        if(row->chars[cx] == '\t')
        {
            cur_rx += (JDEDIT_TAB_STOP - 1) - (cur_rx % JDEDIT_TAB_STOP);
        }

        cur_rx++;

        if(cur_rx > rx)
        {
            return cx;
        }
    }

    return cx;
}

void editorUpdateRow(erow *row)
{
    int tabs = 0;

    int j;
    int idx = 0;

    for(j = 0; j < row->size; ++j)
    {
        if(row->chars[j] == '\t')
        {
            ++tabs;
        }
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(JDEDIT_TAB_STOP - 1) + 1);

    for(j = 0; j < row->size; ++j)
    {
        if(row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while(idx % JDEDIT_TAB_STOP != 0)
            {
                row->render[idx++] = ' ';
            }
        }
        else
        {
            row->render[idx++] = row->chars[j];
        }
    }

    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(&E, row);
}

void editorInsertRow(int at, char *s, size_t len)
{
    if(at < 0 || at > E.numrows)
    {
        return;
    }

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow)*(E.numrows - at));

    for(int j = at + 1; j <= E.numrows; ++j)
    {
        E.row[j].idx++;
    }


    E.row[at].idx = at;

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = 0;

    E.row[at].hl = NULL;

    E.row[at].hl_open_comment = 0;

    editorUpdateRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

void editorFreeRow(erow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(int at)
{
    if(at < 0 || at >= E.numrows)
    {
        return;
    }

    editorFreeRow(&E.row[at]);

    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));

    for(int j = at; j < E.numrows - 1; ++j)
    {
        E.row[j].idx--;
    }

    E.numrows--;
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c)
{
    if(at < 0 || at > row->size)
    {
        at = row->size;
    }

    row->chars = realloc(row->chars, row->size + 2);

    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);

    ++row->size;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at)
{
    if(at < 0 || at >= row->size)
    {
        return;
    }

    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

//==============================================================================
// Editor Operations
//==============================================================================

void editorInsertChar(int c)
{
    if(E.cy == E.numrows)
    {
        editorInsertRow(E.numrows, "", 0);
    }

    editorRowInsertChar(&E.row[E.cy], E.cx, c);

    ++E.cx;
}

void editorInsertNewline()
{
    if(E.cx == 0)
    {
        editorInsertRow(E.cy, "", 0);
    }
    else
    {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }

    E.cy++;
    E.cx = 0;
}

void editorDelChar()
{
    if(E.cy == E.numrows)
    {
        return;
    }

    if(E.cx == 0 && E.cy == 0)
    {
        return;
    }

    erow *row = &E.row[E.cy];

    if(E.cx > 0)
    {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    else
    {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

//==============================================================================
// File IO
//==============================================================================

char *editorRowsToString(int *buflen)
{
    int totlen = 0;
    int j;

    for(j = 0; j < E.numrows; ++j)
    {
        totlen += E.row[j].size + 1;
    }

    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;

    for(j = 0; j < E.numrows; ++j)
    {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        ++p;
    }

    return buf;
}

void editorOpen(char *filename)
{
    if(E.filename && E.dirty)
    {
        char *response = editorPrompt("Warning! Unsaved changes. Close anyway? y/n: %s",
                                      NULL);

        if(!(strcmp(response, "y") == 0 || strcmp(response, "Y") == 0))
        {
            return;
        }
    }

    editorClose();

    E.filename = strdup(filename);

    editorSelectSyntaxHighlight(&E);

    FILE *fp = fopen(filename, "r");
    if(!fp)
    {
        // File did not exist. Create it.
        fp = fopen(filename, "w");

        if(!fp)
        {
            // File could not be opened.
            die("fopen");
        }
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    ssize_t bytes_read = 0;

    while((linelen = getline(&line, &linecap, fp)) != -1)
    {
        bytes_read += linelen;
        while(linelen > 0 &&
              (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
        {
            linelen--;
        }

        editorInsertRow(E.numrows, line, linelen);
    }

    free(line);
    fclose(fp);
    E.dirty = 0;

    editorSetStatusMessage("Opened File: %.20s - %d bytes read", filename, bytes_read);
}

void editorClose()
{
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;

    while(E.numrows)
    {
        editorDelRow(E.numrows - 1);
    }

    free(E.row);

    E.row = NULL;
    E.dirty = 0;

    free(E.filename);
    E.filename = NULL;

    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    E.syntax = NULL;
}

void editorSave()
{
    if(E.filename == NULL)
    {
        E.filename = editorPrompt("Save as: %s", NULL);

        if(E.filename == NULL)
        {
            editorSetStatusMessage("Save Aborted");
            return;
        }

        editorSelectSyntaxHighlight(&E);
    }

    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if(fd != -1)
    {
        if(ftruncate(fd, len) != -1)
        {
            if(write(fd, buf, len) == len)
            {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("Wrote File: %.20s - %d bytes written",
                                       E.filename, len);
                return;
            }
        }
        close(fd);
    }

    free(buf);

    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

//==============================================================================
// Find
//==============================================================================

void editorFindCallback(char *query, int key)
{
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if(saved_hl)
    {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if(key == '\r' || key == '\x1b')
    {
        last_match = -1;
        direction = 1;

        return;
    }
    else if(key == ARROW_RIGHT || key == ARROW_DOWN)
    {
        direction = 1;
    }
    else if(key == ARROW_LEFT || key == ARROW_UP)
    {
        direction = -1;
    }
    else
    {
        last_match = -1;
        direction = 1;
    }

    if(last_match == -1)
    {
        direction = 1;
    }

    int current = last_match;
    int i;

    for(i = 0; i < E.numrows; ++i)
    {
        current += direction;

        if(current == -1)
        {
            current = E.numrows - 1;
        }
        else if(current == E.numrows)
        {
            current = 0;
        }

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);

        if(match)
        {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind()
{
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editorPrompt("Search: %s", editorFindCallback);

    if(query)
    {
        free(query);
    }
    else
    {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

//==============================================================================
// Input
//==============================================================================

char *editorPrompt(char *prompt, void (*callback)(char *, int))
{
    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while(1)
    {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = terminalReadKey();

        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        {
            if(buflen != 0)
            {
                buf[--buflen] = '\0';
            }
        }
        else if(c == '\x1b')
        {
            editorSetStatusMessage("");

            if(callback)
            {
                callback(buf, c);
            }

            free(buf);
            return NULL;
        }
        else if(c == '\r')
        {
            if(buflen != 0)
            {
                editorSetStatusMessage("");

                if(callback)
                {
                    callback(buf, c);
                }

                return buf;
            }
        }
        else if(!iscntrl(c) && c < 128)
        {
            if(buflen == bufsize - 1)
            {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if(callback)
        {
            callback(buf, c);
        }
    }
}

void editorMoveCursor(int key)
{
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key)
    {
    case ARROW_LEFT:
        if(E.cx != 0)
        {
            E.cx--;
        }
        else if(E.cy > 0)
        {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if(row && E.cx < row->size)
        {
            E.cx++;
        }
        else if(row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    case ARROW_UP:
        if(E.cy != 0)
        {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if(E.cy < E.numrows)
        {
            E.cy++;
        }
        break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if(E.cx > rowlen)
    {
        E.cx = rowlen;
    }
}

void editorProcessKeypress()
{
    int c;

    c = terminalReadKey();

    switch(c)
    {
    case '\r':
        editorInsertNewline();
        break;
    case CTRL_KEY('q'):
        if(E.dirty)
        {
            char *response = editorPrompt("Warning! Unsaved changes. Exit anyway? y/n: %s",
                                          NULL);

            if(!(strcmp(response, "y") == 0 || strcmp(response, "Y") == 0))
            {
                if(response)
                {
                    free(response);
                }
                return;
            }

            free(response);
        }
        terminalWrite("\x1b[2J", 4);
        terminalWrite("\x1b[H", 3);

        terminalDisableRawMode(&E);

        exit(0);
        break;

    case CTRL_KEY('s'):
        editorSave();
        break;

    case CTRL_KEY('o'):
    {
        char *filename = editorPrompt("Open file: %s", NULL);

        if(filename)
        {
            editorOpen(filename);

            free(filename);
        }
    }
    break;

    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        if(E.cy < E.numrows)
        {
            E.cx = E.row[E.cy].size;
        }
        break;

    case CTRL_KEY('f'):
        editorFind();
        break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        if(c == DEL_KEY)
        {
            editorMoveCursor(ARROW_RIGHT);
        }
        editorDelChar();
        break;

    case PAGE_UP:
    case PAGE_DOWN:
    {
        if(c == PAGE_UP)
        {
            E.cy = E.rowoff;
        }
        else if(c == PAGE_DOWN)
        {
            E.cy = E.rowoff + E.screenRows - 1;

            if(E.cy > E.numrows)
            {
                E.cy = E.numrows;
            }
        }

        int times = E.screenRows;
        while(times--)
        {
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
    }
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

    default:
        editorInsertChar(c);
        break;
    }
}

//==============================================================================
// Output
//==============================================================================

void editorScroll()
{
    E.rx = 0;

    if(E.cy < E.numrows)
    {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    if(E.cy < E.rowoff)
    {
        E.rowoff = E.cy;
    }

    if(E.cy >= E.rowoff + E.screenRows)
    {
        E.rowoff = E.cy - E.screenRows + 1;
    }

    if(E.rx < E.coloff)
    {
        E.coloff = E.rx;
    }

    if(E.rx >= E.coloff + E.screenCols)
    {
        E.coloff = E.rx - E.screenCols + 1;
    }
}

void editorDrawRows(struct appendBuffer *ab)
{
    int y;

    for(y = 0; y < E.screenRows; ++y)
    {
        int filerow = y + E.rowoff;

        if(E.linum_mode)
        {
            char buf[16];

            memset(buf, ' ', sizeof(buf));

            if(filerow < E.numrows)
            {
                snprintf(buf, sizeof(buf), "%0*d ", E.linum_width - 1, filerow + 1);
            }

            abAppend(ab, buf, E.linum_width);
        }

        if(filerow >= E.numrows)
        {
            if(E.numrows == 0 && y == E.screenRows / 3)
            {
                char welcome[80];

                int welcomelen = snprintf(welcome, sizeof(welcome),
                                          "JDEDIT editor -- version %s", PACKAGE_VERSION);
                if(welcomelen > E.screenCols)
                {
                    welcomelen = E.screenCols;
                }

                int padding = (E.screenCols - welcomelen) / 2;

                if(padding)
                {
                    abAppend(ab, "~", 1);
                    padding--;
                }

                while(padding--)
                {
                    abAppend(ab, " ", 1);
                }

                abAppend(ab, welcome, welcomelen);
            }
            else
            {
                abAppend(ab, "~", 1);
            }
        }
        else
        {
            int len = E.row[filerow].rsize - E.coloff;

            if(len < 0)
            {
                len = 0;
            }

            if(len > E.screenCols)
            {
                len = E.screenCols;
            }

            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;

            int j;

            for(j = 0; j < len; ++j)
            {
                if(iscntrl(c[j]))
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';

                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);

                    if(current_color != -1)
                    {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                }
                else if(hl[j] == HL_NORMAL)
                {
                    if(current_color != -1)
                    {
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1);
                }
                else
                {
                    int color = editorSyntaxToColor(hl[j]);

                    if(color != current_color)
                    {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf),"\x1b[%dm", color);
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

void editorDrawStatusBar(struct appendBuffer *ab)
{
    abAppend(ab, "\x1b[7m", 4);

    char status[80];
    char rstatus[80];

    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.filename ? E.filename : "[No Name]", E.numrows,
                       E.dirty ? "(modified)" : "");

    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
                        E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);


    if(len > E.screenCols + E.linum_width)
    {
        len = E.screenCols + E.linum_width;
    }

    abAppend(ab, status, len);

    while (len < E.screenCols + E.linum_width)
    {
        if(E.screenCols + E.linum_width - len == rlen)
        {
            abAppend(ab, rstatus, rlen);
            break;
        }
        else
        {
            abAppend(ab, " ", 1);
            ++len;
        }
    }

    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct appendBuffer *ab)
{
    abAppend(ab, "\x1b[K", 3);

    int msglen = strlen(E.statusmsg);

    if(msglen > E.screenCols + E.linum_width)
    {
        msglen = E.screenCols + E.linum_width;
    }

    if(msglen && time(NULL) - E.statusmsg_time < 5)
    {
        abAppend(ab, E.statusmsg, msglen);
    }
}

void editorRefreshScreen()
{
    if(terminalGetWindowSize(&E.windowRows, &E.windowCols) == -1)
    {
        die("getWindowSize");
    }

    E.screenRows = E.windowRows - 2;
    E.screenCols = E.windowCols - E.linum_width;

    if(E.linum_mode)
    {
        char buf[16];

        snprintf(buf, sizeof(buf), "%d ", E.numrows);

        E.linum_width = strlen(buf);

        E.screenCols = E.windowCols - E.linum_width;
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

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
             (E.rx - E.coloff) + 1 + E.linum_width);

    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    terminalWrite(ab.b, ab.len);

    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);

    va_end(ap);

    E.statusmsg_time = time(NULL);
}

//==============================================================================
// Init
//==============================================================================

void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;

    E.linum_width = 0;
    E.linum_mode = 1;

    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;

    E.filename = NULL;

    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    E.syntax = NULL;

    if(terminalGetWindowSize(&E.windowRows, &E.windowCols) == -1)
    {
        die("getWindowSize");
    }

    E.screenRows = E.windowRows - 2;
    E.screenCols = E.windowCols - E.linum_width;
}

int main(int argc, char **argv)
{
    terminalEnableRawMode(&E);
    initEditor();
    if(argc >= 2)
    {
        editorOpen(argv[1]);
    }

    while(1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    terminalDisableRawMode(&E);

    return 0;
}

//==============================================================================
// End of file
//==============================================================================
