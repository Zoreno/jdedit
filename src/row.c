#include "row.h"

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

#include "editor.h"

#define JDEDIT_TAB_STOP 4 // TODO: Move to editor

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


void editorUpdateRow(struct editorConfig *conf, erow *row)
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

    editorUpdateSyntax(conf, row);
}

void editorFreeRow(erow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorRowInsertChar(struct editorConfig *conf, erow *row, int at, int c)
{
    if(at < 0 || at > row->size)
    {
        at = row->size;
    }

    row->chars = realloc(row->chars, row->size + 2);

    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);

    ++row->size;
    row->chars[at] = c;
    editorUpdateRow(conf, row);
    conf->activeBuffer->dirty++;
}

void editorRowAppendString(struct editorConfig *conf, erow *row, char *s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(conf, row);
    conf->activeBuffer->dirty++;
}

void editorRowDelChar(struct editorConfig *conf, erow *row, int at)
{
    if(at < 0 || at >= row->size)
    {
        return;
    }

    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(conf, row);
    conf->activeBuffer->dirty++;
}
