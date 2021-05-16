// 2007-01-30  Paolo Carlini  <pcarlini@suse.de>

// Copyright (C) 2007 Free Software Foundation, Inc.
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

#include <new>
#include <typeinfo>
#include <exception>
#include <cstring>
#include <testsuite_hooks.h>

// libstdc++/14493
void test01() 
{
  bool test __attribute__((unused)) = true;
  using namespace std;

  bad_alloc ba;
  VERIFY( !strcmp(ba.what(), "std::bad_alloc") );

  bad_cast bc;
  VERIFY( !strcmp(bc.what(), "std::bad_cast") );

  bad_typeid bt;
  VERIFY( !strcmp(bt.what(), "std::bad_typeid") );

  exception e;
  VERIFY( !strcmp(e.what(), "std::exception") );

  bad_exception be;
  VERIFY( !strcmp(be.what(), "std::bad_exception") );
}

int main()
{
  test01();
  return 0;
}
