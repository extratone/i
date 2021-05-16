// 2003-04-12  Paolo Carlini  <pcarlini at unitus dot it>

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

// stdio_filebuf.h

#include <ext/stdio_filebuf.h>
#include <cstdio>
#include <fstream>
#include <testsuite_hooks.h>

// Small stack-based buffers (i.e., using _M_unbuf) were not flushed
// out by _M_really_overflow upon overflow.
void test01()
{
  using namespace std;
  bool test __attribute__((unused)) = true;

  const char* name = "tmp_file1";
  FILE* file = fopen(name, "w");
  {
    using namespace __gnu_cxx;
    stdio_filebuf<char> sbuf(file, ios_base::out, 2); 
    sbuf.sputc('T');
    sbuf.sputc('S');
    sbuf.sputc('P');
  }
  fclose(file);

  filebuf fbuf;
  fbuf.open(name, ios_base::in);
  char buf[10];
  streamsize n = fbuf.sgetn(buf, sizeof(buf));	
  fbuf.close();
  
  VERIFY( n == 3 );
  VERIFY( !memcmp(buf, "TSP", 3) );
}

int main()
{
  test01();
}
