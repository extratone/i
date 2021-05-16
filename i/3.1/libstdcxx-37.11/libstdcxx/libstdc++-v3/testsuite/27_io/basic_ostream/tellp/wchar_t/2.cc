// Copyright (C) 2005 Free Software Foundation
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
//

#include <sstream>
#include <ostream>
#include <testsuite_hooks.h>

void test01()
{
  using namespace std;
  typedef std::wstringbuf::pos_type        pos_type;
  typedef std::wstringbuf::off_type        off_type;
  bool test __attribute__((unused)) = true;

  // tellp
  wostringstream ost;
  pos_type pos1;
  pos1 = ost.tellp();
  VERIFY( pos1 == pos_type(off_type(0)) );
  ost << L"RZA ";
  pos1 = ost.tellp();
  VERIFY( pos1 == pos_type(off_type(4)) );
  ost << L"ghost dog: way of the samurai";
  pos1 = ost.tellp();
  VERIFY( pos1 == pos_type(off_type(33)) );
}                                    

int main()
{
  test01();
  return 0;
}
