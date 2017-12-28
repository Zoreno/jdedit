#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "key.h"
#include "editor.h"

void terminalDisableRawMode(struct editorConfig *conf);

void terminalEnableRawMode(struct editorConfig *conf);

int terminalReadKey();

int terminalGetCursorPosition(int *rows, int *cols);

int terminalGetWindowSize(int *rows, int *cols);

void terminalWrite(const char *s, int len);

#endif
