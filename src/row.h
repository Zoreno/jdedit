#ifndef _ROW_H
#define _ROW_H

#include <sys/types.h>

struct editorConfig;

typedef struct erow
{
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
    int hl_open_comment;
} erow;

int editorRowCxToRx(erow *row, int cx);
int editorRowRxToCx(erow *row, int rx);

void editorUpdateRow(struct editorConfig *conf, erow *row);
void editorFreeRow(erow *row);
void editorRowInsertChar(struct editorConfig *conf, erow *row, int at, int c);
void editorRowAppendString(struct editorConfig *conf, erow *row, char *s, size_t len);
void editorRowDelChar(struct editorConfig *conf, erow *row, int at);

#endif
