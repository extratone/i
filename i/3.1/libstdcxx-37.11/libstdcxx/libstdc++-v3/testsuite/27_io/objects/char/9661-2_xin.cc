// 2003-04-30  Petur Runolfsson <peturr02@ru.is>

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

#include <testsuite_hooks.h>
#include <cstdio>
#include <iostream>

void test02()
{
  using namespace std;

  bool test __attribute__((unused)) = true;

  int c1 = fgetc(stdin);
  int c2 = cin.rdbuf()->sputbackc(c1);
  VERIFY( c2 == c1 );
  
  int c3 = fgetc(stdin);
  VERIFY( c3 == c1 );
  ungetc(c3, stdin);
  
  int c4 = cin.rdbuf()->sgetc();
  VERIFY( c4 == c3 );
}

int main()
{
  test02();
  return 0;
}
