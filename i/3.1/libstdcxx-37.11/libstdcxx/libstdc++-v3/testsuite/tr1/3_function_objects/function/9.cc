// 2005-01-15 Douglas Gregor <dgregor@cs.indiana.edu>
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

// 3.7.2 polymorphic function object wrapper
#include <tr1/functional>
#include <testsuite_hooks.h>
#include <testsuite_tr1.h>

using namespace __gnu_test;

bool test __attribute__((unused)) = true;

// Put function objects into a void-returning function<> wrapper
void test09()
{
  using std::tr1::function;
  using std::tr1::ref;
  using std::tr1::cref;

  int (X::*X_foo_c)() const = &X::foo_c;
  function<void(X&)> f(&X::bar);
  f = &X::foo;
  f = ref(X_foo_c);
  f = cref(X_foo_c);

  function<void(float)> g = &truncate_float;
  g = do_truncate_float_t();
}

int main()
{
  test09();

  VERIFY( do_truncate_double_t::live_objects == 0 );
  VERIFY( do_truncate_float_t::live_objects == 0 );

  return 0;
}
