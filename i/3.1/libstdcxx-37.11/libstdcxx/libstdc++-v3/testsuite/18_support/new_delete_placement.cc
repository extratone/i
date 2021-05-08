// 2002-07-24 Benjamin Kosnik

// Copyright (C) 2002, 2004 Free Software Foundation
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

// 18.4.1.3 - Placement forms

#include <new>
#include <testsuite_hooks.h>

// libstdc++/7286
void test01()
{
  void* pc = new char;
  void* pa = new char[10];
  void* tmp = NULL;
  operator delete(pc, tmp);
  operator delete[](pa, tmp);
}

int main()
{
  test01();
  return 0;
}
