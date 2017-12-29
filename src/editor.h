#ifndef _EDITOR_H
#define _EDITOR_H

#include "syntax.h"
#include "row.h"

#include <time.h>
#include <termios.h>

struct editorConfig;

struct buffer
{
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
};

struct editorConfig
{
    struct buffer *activeBuffer;

    struct buffer **buffers;
    int numBuffers;

    int curBuffer;
    
    int windowRows;
    int windowCols;

    int screenRows;
    int screenCols;

    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
};

void editorInsertRow(struct editorConfig *conf, int at, char *s, size_t len);
void editorDelRow(struct editorConfig *conf, int at);
void editorInsertChar(struct editorConfig *conf, int c);
void editorInsertNewline(struct editorConfig *conf);
void editorDelChar(struct editorConfig *conf);

void editorCreateBuffer(struct editorConfig *conf, struct buffer **buf_ptr);

/*
void editorSwitchBufferByName(struct editorConfig *conf,
                              const char *name,
                              struct buffer **buf_ptr);
*/

void editorNextBuffer(struct editorConfig *conf, struct buffer **buf_ptr);

void editorPrevBuffer(struct editorConfig *conf, struct buffer **buf_ptr);

void editorFirstBuffer(struct editorConfig *conf, struct buffer **buf_ptr);

void editorLastBuffer(struct editorConfig *conf, struct buffer **buf_ptr);


void initBuffer(struct buffer *buffer);
void freeBuffer(struct buffer *buffer);

#endif
