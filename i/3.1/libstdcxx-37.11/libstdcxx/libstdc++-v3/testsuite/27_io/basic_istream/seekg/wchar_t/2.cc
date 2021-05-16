// Copyright (C) 2004 Free Software Foundation
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

// 27.6.1.3 unformatted input functions
// DR 60 -- seekg does not effect calls to gcount

#include <istream>
#include <sstream>
#include <testsuite_hooks.h>

void test01()
{
  using namespace std;
  bool test __attribute__((unused)) = true;

  wistringstream ist(L"the lamb lies down on broadway");
  wios::pos_type pos = ist.tellg();
  ist.ignore(4);
  int count1 = ist.gcount();
  ist.seekg(pos);
  int count2 = ist.gcount();
  VERIFY( count1 == count2 );
  ist.seekg(wios::off_type(pos), wios::beg);
  count2 = ist.gcount();
  VERIFY( count1 == count2 );
}

int main()
{
  test01();
  return 0;
}
