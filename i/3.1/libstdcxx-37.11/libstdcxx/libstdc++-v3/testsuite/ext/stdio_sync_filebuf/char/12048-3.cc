// 2003-05-01 Petur Runolfsson <peturr02@ru.is>

// Copyright (C) 2003 Free Software Foundation
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

#include <ext/stdio_sync_filebuf.h>
#include <testsuite_hooks.h>

// libstdc++/12048
void test04()
{
  bool test __attribute__((unused)) = true;
  const char* name = "cin_unget-1.txt";

  std::FILE* file = std::fopen(name, "r");
  __gnu_cxx::stdio_sync_filebuf<char> sbuf(file);
  char buf[2];
  VERIFY( sbuf.sgetn(buf, 2) == 2 );
  int c2 = sbuf.sungetc();
  VERIFY( c2 != EOF );
  int c3 = sbuf.sbumpc();
  VERIFY( c3 == std::char_traits<char>::to_int_type(buf[1]) );

  std::fclose(file);
}

int main ()
{
  test04();
  return 0;
}
