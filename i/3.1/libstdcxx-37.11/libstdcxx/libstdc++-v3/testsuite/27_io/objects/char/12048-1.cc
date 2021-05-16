// Derived from libstdc++/12048 by LJR <ljrittle@acm.org> with
// reminder from Petur Runolfsson <peturr02@ru.is>.

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

#include <iostream>
#include <testsuite_hooks.h>

void
test01()
{
  std::freopen("cin_unget-1.txt", "r", stdin);

  char c1;
  char c2;
  std::cin.get(c1);
  std::cin.unget();
  std::cin.get(c2);
  VERIFY( std::cin.good() );
  VERIFY( c1 == c2 );
}

int main(void)
{
  test01();
  return 0;
}
