// 2003-05-01  Petur Runolfsson  <peturr02@ru.is>

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
#include <locale>
#include <string>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <algorithm>
#include <testsuite_hooks.h>

// This tests the handling of multibyte characters by wcin and wcout.
void test09()
{
  using namespace std;

  locale loc("");
  locale::global(loc);
	
  wcout.imbue(loc);
  wcin.imbue(loc);

  wcout << L"Current locale is: \'" << loc.name().c_str() << L"\'\n";
  wcout << L"Please enter your name: ";

  wstring str;
  getline(wcin, str);

  wcout << str << endl;
  wcout << str.size() << endl;

  transform(str.begin(), str.end(),
	    ostream_iterator<wchar_t, wchar_t>(wcout), towupper);
  wcout << endl;
  transform(str.begin(), str.end(),
	    ostream_iterator<wchar_t, wchar_t>(wcout), towlower);
  wcout << endl << hex << showbase;
  copy(str.begin(), str.end(),
       ostream_iterator<wint_t, wchar_t>(wcout, L" "));
  wcout << endl;
}

int main()
{
  test09();
  return 0;
}
