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

#include <cwchar> // for wcslen
#include <istream>
#include <sstream>
#include <testsuite_hooks.h>

// [bug] istream::getline(char*,streamsize) still broken
// http://gcc.gnu.org/ml/libstdc++/2000-07/msg00126.html
// brent verner <brent at rcfile dot org
void
test06()
{
  using namespace std;

  bool test __attribute__((unused)) = true;
  const streamsize it = 5;
  wchar_t tmp[it];
  const wchar_t* str_lit = L"abcd\n";

  wstringbuf strbuf(str_lit, std::ios_base::in);
  wistream istr(&strbuf);
  
  istr.getline(tmp,it); 
  VERIFY( istr.gcount() == it );  // extracted whole string
  VERIFY( wcslen(tmp) == 4 );     // stored all but '\n'
  VERIFY( !istr.eof() );          // extracted up to but not eof
  VERIFY( !istr.fail() );         // failbit not set
  
  wchar_t c = L'z';
  istr.get(c);
  VERIFY( c == L'z' );
  VERIFY( istr.eof() );
}

int 
main()
{
  test06();
  return 0;
}
