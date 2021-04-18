/**
 * @file append_buffer.h
 * @author Joakim Bertils
 * @version 0.1
 * @date 2021-04-18
 *
 * @brief Append buffer interface.
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

#ifndef _APPEND_BUFFER_H
#define _APPEND_BUFFER_H

struct appendBuffer {
  char *b;
  int len;
};

void abInit(struct appendBuffer *ab);
void abAppend(struct appendBuffer *ab, const char *s, int len);
void abFree(struct appendBuffer *ab);

#endif
