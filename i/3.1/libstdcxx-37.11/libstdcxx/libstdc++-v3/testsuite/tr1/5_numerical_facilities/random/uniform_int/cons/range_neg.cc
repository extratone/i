// 2006-06-04  Stephen M. Webb <stephen.webb@bregmasoft.com>
//
// Copyright (C) 2006 Free Software Foundation, Inc.
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

// { dg-do run { xfail *-*-* } }
// { dg-options "-D_GLIBCXX_DEBUG" }

// 5.1.7.1 class template uniform_int
// 5.1.7.1 [2] constructors and member functions

#include <tr1/random>
#include <testsuite_hooks.h>

void
test01() 
{ 
  using namespace std::tr1;

  uniform_int<int> u(20, 1);
}

int main()
{
  test01();
  return 0;
}
