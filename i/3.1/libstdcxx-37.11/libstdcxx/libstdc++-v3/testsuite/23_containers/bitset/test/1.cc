// 2001-06-03 pme

// Copyright (C) 2001, 2002, 2003 Free Software Foundation, Inc.
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

// 23.3.5.2 bitset members

#include <bitset>
#include <stdexcept>
#include <sstream>
#include <testsuite_hooks.h>

void 
test01(void)
{
  bool test __attribute__((unused)) = true;
  const size_t n1 = 5;

  // the other 22 member functions should be in here too...
  try {
    std::bitset<n1> five_bits;
    bool unused __attribute__((unused)) = five_bits.test(n1);   // should throw
    VERIFY( false );
  }
  catch(std::out_of_range& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }
  VERIFY( test );
}

int main()
{
  test01();
  return 0;
}
