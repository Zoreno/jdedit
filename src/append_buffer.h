#ifndef _APPEND_BUFFER_H
#define _APPEND_BUFFER_H

struct appendBuffer
{
    char *b;
    int len;
};


void abInit(struct appendBuffer *ab);
void abAppend(struct appendBuffer *ab, const char *s, int len);
void abFree(struct appendBuffer *ab);

#endif
