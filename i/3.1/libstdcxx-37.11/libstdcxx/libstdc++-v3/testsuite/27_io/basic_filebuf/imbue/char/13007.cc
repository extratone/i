// { dg-require-namedlocale "" }

// Copyright (C) 2003, 2005 Free Software Foundation, Inc.
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

// 27.8.1.4 Overridden virtual functions

#include <fstream>
#include <locale>
#include <testsuite_hooks.h>

class Buf : public std::filebuf
{
public:
  std::locale before;
  std::locale after;

protected:
  void imbue(const std::locale& loc)
  {
    before = getloc();

    std::filebuf::imbue(loc);

    after = getloc();
  }
};

// libstdc++/13007
void test01()
{
  bool test __attribute__((unused)) = true;

  Buf buf;
  std::locale loc(std::locale("fr_FR"));

  buf.pubimbue(loc);

  VERIFY( buf.getloc() == loc );
  VERIFY( buf.before == std::locale::classic() );
  VERIFY( buf.after == std::locale::classic() );
}

int main()
{
  test01();
  return 0;
}
