#include "editor.h"

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

#include "row.h"
#include "syntax.h"
#include "editor.h"
#include "row.h"
#include "terminal.h"
#include "key.h"
#include "append_buffer.h"

#define JDEDIT_TAB_STOP 4

void editorInsertRow(struct editorConfig *conf, int at, char *s, size_t len)
{
    if(at < 0 || at > conf->activeBuffer->numrows)
    {
        return;
    }

    conf->activeBuffer->row = realloc(conf->activeBuffer->row,
                                      sizeof(erow) * (conf->activeBuffer->numrows + 1));
    memmove(&conf->activeBuffer->row[at + 1], &conf->activeBuffer->row[at],
            sizeof(erow)*(conf->activeBuffer->numrows - at));

    for(int j = at + 1; j <= conf->activeBuffer->numrows; ++j)
    {
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


void editorDelRow(struct editorConfig *conf, int at)
{
    if(at < 0 || at >= conf->activeBuffer->numrows)
    {
        return;
    }

    editorFreeRow(&conf->activeBuffer->row[at]);

    memmove(&conf->activeBuffer->row[at],
            &conf->activeBuffer->row[at + 1],
            sizeof(erow) * (conf->activeBuffer->numrows - at - 1));

    for(int j = at; j < conf->activeBuffer->numrows - 1; ++j)
    {
        conf->activeBuffer->row[j].idx--;
    }

    conf->activeBuffer->numrows--;
    conf->activeBuffer->dirty++;
}

void editorInsertChar(struct editorConfig *conf, int c)
{
    if(conf->activeBuffer->cy == conf->activeBuffer->numrows)
    {
        editorInsertRow(conf, conf->activeBuffer->numrows, "", 0);
    }

    editorRowInsertChar(conf, &conf->activeBuffer->row[conf->activeBuffer->cy],
                        conf->activeBuffer->cx, c);

    ++conf->activeBuffer->cx;
}

void editorInsertNewline(struct editorConfig *conf)
{
    if(conf->activeBuffer->cx == 0)
    {
        editorInsertRow(conf, conf->activeBuffer->cy, "", 0);
    }
    else
    {
        erow *row = &conf->activeBuffer->row[conf->activeBuffer->cy];
        editorInsertRow(conf, conf->activeBuffer->cy + 1, &row->chars[conf->activeBuffer->cx],
                        row->size - conf->activeBuffer->cx);
        row = &conf->activeBuffer->row[conf->activeBuffer->cy];
        row->size = conf->activeBuffer->cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(conf, row);
    }

    conf->activeBuffer->cy++;
    conf->activeBuffer->cx = 0;
}

void editorDelChar(struct editorConfig *conf)
{
    if(conf->activeBuffer->cy == conf->activeBuffer->numrows)
    {
        return;
    }

    if(conf->activeBuffer->cx == 0 && conf->activeBuffer->cy == 0)
    {
        return;
    }

    erow *row = &conf->activeBuffer->row[conf->activeBuffer->cy];

    if(conf->activeBuffer->cx > 0)
    {
        editorRowDelChar(conf, row, conf->activeBuffer->cx - 1);
        conf->activeBuffer->cx--;
    }
    else
    {
        conf->activeBuffer->cx = conf->activeBuffer->row[conf->activeBuffer->cy - 1].size;
        editorRowAppendString(conf, &conf->activeBuffer->row[conf->activeBuffer->cy - 1],
                              row->chars, row->size);
        editorDelRow(conf, conf->activeBuffer->cy);
        conf->activeBuffer->cy--;
    }
}

void editorCreateBuffer(struct editorConfig *conf, struct buffer **buf_ptr)
{
    conf->buffers = realloc(conf->buffers, sizeof(struct buffer *) * (conf->numBuffers + 1));

    struct buffer *new = malloc(sizeof(struct buffer));

    if(new)
    {
        initBuffer(new);

        conf->buffers[conf->numBuffers] = new;

        conf->numBuffers++;

        if(buf_ptr)
        {
            *buf_ptr = new;
        }
    }
}

void editorNextBuffer(struct editorConfig *conf, struct buffer **buf_ptr)
{
    ++conf->curBuffer;

    if(conf->curBuffer == conf->numBuffers)
    {
        conf->curBuffer = 0;
    }

    conf->activeBuffer = conf->buffers[conf->curBuffer];

    if(buf_ptr)
    {
        *buf_ptr = conf->activeBuffer;;
    }
}

void editorPrevBuffer(struct editorConfig *conf, struct buffer **buf_ptr)
{
    if(conf->curBuffer == 0)
    {
        conf->curBuffer = conf->numBuffers;;
    }

    --conf->curBuffer;

    conf->activeBuffer = conf->buffers[conf->curBuffer];

    if(buf_ptr)
    {
        *buf_ptr = conf->activeBuffer;;
    }
}

void editorFirstBuffer(struct editorConfig *conf, struct buffer **buf_ptr)
{
    conf->curBuffer = 0;

    conf->activeBuffer = conf->buffers[0];

    if(buf_ptr)
    {
        *buf_ptr = conf->activeBuffer;;
    }
}

void editorLastBuffer(struct editorConfig *conf, struct buffer **buf_ptr)
{
    conf->curBuffer = conf->numBuffers - 1;

    conf->activeBuffer = conf->buffers[conf->numBuffers - 1];

    if(buf_ptr)
    {
        *buf_ptr = conf->activeBuffer;;
    }
}


void initBuffer(struct buffer *buffer)
{
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

void freeBuffer(struct buffer *buffer)
{
    // TODO: Fix
}
