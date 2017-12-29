#include "syntax.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", NULL};
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",

    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", NULL
};

char *Python_HL_extensions[] = { ".py", NULL};
char *Python_HL_keywords[] = {
    "and", "assert", "break", "class", "continue", "def", "del", "elif",
    "else", "except", "exec", "finally", "for", "from", "global", "if",
    "import", "in", "is", "lambda", "not", "or", "pass", "print", "raise",
    "return", "try", "while", "with", "yield", NULL
};

static struct editorSyntax HLDB[] =
{
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "Py",
        Python_HL_extensions,
        Python_HL_keywords,
        "#",
        NULL,
        NULL,
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];:", c) != NULL;
}

void editorUpdateSyntax(struct editorConfig *conf, erow *row)
{
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if(conf->activeBuffer->syntax == NULL)
    {
        return;
    }

    char **keywords = conf->activeBuffer->syntax->keywords;

    char *scs = conf->activeBuffer->syntax->singleline_comment_start;

    char *mcs = conf->activeBuffer->syntax->multiline_comment_start;
    char *mce = conf->activeBuffer->syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && conf->activeBuffer->row[row->idx - 1].hl_open_comment);

    int i = 0;

    while(i < row->rsize)
    {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if(scs_len && !in_string && !in_comment)
        {
            if(!strncmp(&row->render[i], scs, scs_len))
            {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if(mcs_len && mce_len && !in_string)
        {
            if(in_comment)
            {
                row->hl[i] = HL_MLCOMMENT;

                if(!strncmp(&row->render[i], mce, mce_len))
                {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                else
                {
                    ++i;
                    continue;
                }
            }
            else if(!strncmp(&row->render[i], mcs, mcs_len))
            {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if(conf->activeBuffer->syntax->flags & HL_HIGHLIGHT_STRINGS)
        {
            if(in_string)
            {
                row->hl[i] = HL_STRING;

                if(c == '\\' && i + 1 < row->rsize)
                {
                    row->hl[i + 1] = HL_STRING;

                    i += 2;

                    continue;
                }

                if(c == in_string)
                {
                    in_string = 0;
                }

                ++i;

                prev_sep = 1;

                continue;
            }
            else
            {
                if(c == '"' || c == '\'')
                {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    ++i;
                    continue;
                }
            }
        }

        if(conf->activeBuffer->syntax->flags & HL_HIGHLIGHT_NUMBERS)
        {
            if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
               (c == '.' && prev_hl == HL_NUMBER))
            {
                row->hl[i] = HL_NUMBER;
                ++i;
                prev_sep = 0;
                continue;
            }
        }

        if(prev_sep)
        {
            int j;

            for(j = 0; keywords[j]; ++j)
            {
                int klen = strlen(keywords[j]);

                int kw2 = keywords[j][klen - 1] == '|';

                if(kw2)
                {
                    klen--;
                }

                if(!strncmp(&row->render[i], keywords[j], klen) &&
                   is_separator(row->render[i + klen]))
                {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }

            if(keywords[j] != NULL)
            {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        ++i;
    }

    int changed = (row->hl_open_comment != in_comment);

    row->hl_open_comment = in_comment;

    if(changed && row->idx + 1 < conf->activeBuffer->numrows)
    {
        editorUpdateSyntax(conf, &conf->activeBuffer->row[row->idx + 1]);
    }
}

int editorSyntaxToColor(int hl)
{
    switch(hl)
    {
    case HL_NUMBER:
        return 31;
    case HL_MLCOMMENT:
    case HL_COMMENT:
        return 32;
    case HL_KEYWORD1:
        return 34;
    case HL_KEYWORD2:
        return 35;
    case HL_STRING:
        return 33;
    case HL_MATCH:
        return 36;
    default:
        return 37;
    }
}

void editorSelectSyntaxHighlight(struct editorConfig *conf)
{
    conf->activeBuffer->syntax = NULL;

    if(conf->activeBuffer->filename == NULL)
    {
        return;
    }

    char *ext = strrchr(conf->activeBuffer->filename, '.');

    for(unsigned int j = 0; j < HLDB_ENTRIES; ++j)
    {
        struct editorSyntax *s = &HLDB[j];

        unsigned int i = 0;

        while(s->filematch[i])
        {
            int is_ext = (s->filematch[i][0] == '.');

            if((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
               (!is_ext && strstr(conf->activeBuffer->filename, s->filematch[i])))
            {
                conf->activeBuffer->syntax = s;

                int filerow;

                for(filerow = 0; filerow < conf->activeBuffer->numrows; ++filerow)
                {
                    editorUpdateSyntax(conf, &conf->activeBuffer->row[filerow]);
                }

                return;
            }
            ++i;
        }
    }
}
