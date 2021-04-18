/**
 * @file append_buffer.c
 * @author Joakim Bertils
 * @version 0.1
 * @date 2021-04-18
 *
 * @brief Data structure for appending text to buffers.
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

#include "append_buffer.h"

#include <stdlib.h>
#include <string.h>

void abInit(struct appendBuffer *ab) {
  ab->b = NULL;
  ab->len = 0;
}

void abAppend(struct appendBuffer *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) {
    return;
  }

  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct appendBuffer *ab) { free(ab->b); }
