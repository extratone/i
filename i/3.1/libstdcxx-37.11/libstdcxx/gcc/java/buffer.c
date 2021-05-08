/* A "buffer" utility type.
   Copyright (C) 1998, 2003 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* Written by Per Bothner <bothner@cygnus.com>, July 1998. */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "buffer.h"

/* Grow BUFP so there is room for at least SIZE more bytes. */

void
buffer_grow (struct buffer *bufp, int size)
{
  if (bufp->limit - bufp->ptr >= size)
    return;
  if (bufp->data == 0)
    {
      if (size < 120)
	size = 120;
      bufp->data = XNEWVEC (unsigned char, size);
      bufp->ptr = bufp->data;
    }
  else
    {
      int index = bufp->ptr - bufp->data;
      size += 2 * (bufp->limit - bufp->data);
      bufp->data = xrealloc (bufp->data, size);
      bufp->ptr = bufp->data + index;
    }
  bufp->limit = bufp->data + size;
}
