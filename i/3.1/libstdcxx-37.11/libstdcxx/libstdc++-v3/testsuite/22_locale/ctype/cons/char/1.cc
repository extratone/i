// 2000-02-16 bkoz

// Copyright (C) 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
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

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

// 22.2.1.3.2 ctype<char> members

#include <locale>
#include <testsuite_hooks.h>

// Dietmar K�hl via Peter Schmid 
class comma_ctype: public std::ctype<char>
{
public:
  comma_ctype(): std::ctype<char>() { }
  comma_ctype(const std::ctype_base::mask* m): std::ctype<char>(m) { }

  const mask* 
  get_classic_table()
  { return std::ctype<char>::classic_table(); }

  const mask* 
  get_table()
  { return this->table(); }
}; 

void test01()
{
  using namespace std;
  bool test __attribute__((unused)) = true; 

  comma_ctype obj;
  const ctype_base::mask* tmp = obj.get_classic_table();

  comma_ctype obj2(tmp);
  const ctype_base::mask* ctable = obj2.get_table();
  VERIFY ( tmp == ctable );
}

int main() 
{
  test01();
  return 0;
}
