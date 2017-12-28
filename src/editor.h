#ifndef _EDITOR_H
#define _EDITOR_H

#include "syntax.h"
#include "row.h"

#include <time.h>
#include <termios.h>

struct editorConfig
{
    int cx;
    int cy;
    int rx;
    int rowoff;
    int coloff;

    int windowRows;
    int windowCols;

    int screenRows;
    int screenCols;

    int linum_width;
    int linum_mode;

    int numrows;
    erow *row;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct termios orig_termios;
};

#endif
