// Copyright (C) 2003 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

#include <cstdio>
#include <fstream>
#include <testsuite_performance.h>

int main()
{
  using namespace std;
  using namespace __gnu_test;

  time_counter time;
  resource_counter resource;
  const unsigned long count = 1ul << 30;

  // C unlocked
  FILE* fpi = fopen("/dev/zero", "r");
  FILE* fpo = fopen("/dev/null", "w");
  start_counters(time, resource);
  for (unsigned long i = 0; i < count; ++i)
    {
      int c = getc_unlocked(fpi);
      if (c == EOF || putc_unlocked(c, fpo) == EOF)
	break;
    }
  stop_counters(time, resource);
  fclose(fpi);
  fclose(fpo);
  report_performance(__FILE__, "C unlocked", time, resource);
  clear_counters(time, resource);

  // C++
  filebuf in;
  in.open("/dev/zero", ios::in);
  filebuf out;
  out.open("/dev/null", ios::out);
  start_counters(time, resource);
  for (unsigned long i = 0; i < count; ++i)
    {
      int c = in.sbumpc();
      if (c == EOF || out.sputc(c) == EOF)
	break;
    }
  stop_counters(time, resource);
  in.close();
  out.close();
  report_performance(__FILE__, "C++", time, resource);

  return 0;
}
