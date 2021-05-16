// 2005-07-11  Paolo Carlini  <pcarlini@suse.de>

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
// USA.

// 27.6.2.5.2  Arithmetic inserters

#include <sstream>
#include <testsuite_hooks.h>

void test01()
{
  using namespace std;
  bool test __attribute__((unused)) = true;

  wstringstream ostr1, ostr2, ostr3, ostr4;

  ostr1.setf(ios_base::oct);
  ostr1.setf(ios_base::hex);

  short s = -1;
  ostr1 << s;
  VERIFY( ostr1.str() == L"-1" );

  ostr2.setf(ios_base::oct);
  ostr2.setf(ios_base::hex);

  int i = -1;
  ostr2 << i;
  VERIFY( ostr2.str() == L"-1" );

  ostr3.setf(ios_base::oct);
  ostr3.setf(ios_base::hex);

  long l = -1;
  ostr3 << l;
  VERIFY( ostr3.str() == L"-1" );

#ifdef _GLIBCXX_USE_LONG_LONG
  ostr4.setf(ios_base::oct);
  ostr4.setf(ios_base::hex);

  long long ll = -1LL;
  ostr4 << ll;
  VERIFY( ostr4.str() == L"-1" );
#endif
}

int main()
{
  test01();
  return 0;
}
