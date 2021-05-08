// { dg-do compile }
// 2001-08-15  Benjamin Kosnik  <bkoz@redhat.com>

// Copyright (C) 2001 Free Software Foundation
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

// 22.2.4 The collate category

#include <locale>

// Should be able to instantiate this for other types besides char, wchar_t
class gnu_collate: public std::collate<unsigned char> 
{ };

void test02()
{ 
  gnu_collate facet01;
}

int main()
{
  test02();
  return 0;
}
