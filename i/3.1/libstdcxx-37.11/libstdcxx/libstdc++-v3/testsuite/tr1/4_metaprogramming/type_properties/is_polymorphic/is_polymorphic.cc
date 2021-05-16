// 2005-03-03  Paolo Carlini  <pcarlini@suse.de>
//
// Copyright (C) 2005 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
// USA.

// 4.5.3 Type properties

#include <tr1/type_traits>
#include <iostream>
#include <testsuite_hooks.h>
#include <testsuite_tr1.h>

void test01()
{
  bool test __attribute__((unused)) = true;
  using std::tr1::is_polymorphic;
  using namespace __gnu_test;

  // Positive tests.
  VERIFY( (test_category<is_polymorphic, PolymorphicClass>(true)) );
  VERIFY( (test_category<is_polymorphic, DerivedPolymorphic>(true)) );
  VERIFY( (test_category<is_polymorphic, AbstractClass>(true)) );
  VERIFY( (test_category<is_polymorphic, std::iostream>(true)) );
  VERIFY( (test_category<is_polymorphic, std::streambuf>(true)) );

  // Negative tests.
  VERIFY( (test_category<is_polymorphic, void>(false)) );
  VERIFY( (test_category<is_polymorphic, int (int)>(false)) );
  VERIFY( (test_category<is_polymorphic, int&>(false)) );
  VERIFY( (test_category<is_polymorphic, EnumType>(false)) );
  VERIFY( (test_category<is_polymorphic, ClassType>(false)) );
  VERIFY( (test_category<is_polymorphic, DerivedType>(false)) );
}

int main()
{
  test01();
  return 0;
}
