/**
 * @file row.h
 * @author Joakim Bertils
 * @version 0.1
 * @date 2021-04-18
 *
 * @brief Helper module for handling editor rows.
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

#ifndef _ROW_H
#define _ROW_H

#include <sys/types.h>

typedef struct editorConfig editorConfig_t;

typedef struct erow {
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

void editorUpdateRow(editorConfig_t *conf, erow *row);
void editorFreeRow(erow *row);
void editorRowInsertChar(editorConfig_t *conf, erow *row, int at, int c);
void editorRowAppendString(editorConfig_t *conf, erow *row, char *s,
                           size_t len);
void editorRowDelChar(editorConfig_t *conf, erow *row, int at);

#endif
