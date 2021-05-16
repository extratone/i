// 2005-02-13  Paolo Carlini  <pcarlini@suse.de>

// Copyright (C) 2005 Free Software Foundation
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

// 26.5 C Library

#include <cmath>
#include <testsuite_hooks.h>

template<typename T>
  void test01_do()
  {
    using namespace std;
    bool test __attribute__((unused)) = true;
    
    VERIFY( pow(T(1.0), 0) == T(1.0) );
    VERIFY( pow(T(2.0), 0) == T(1.0) );
    VERIFY( pow(T(-1.0), 0) == T(1.0) );
    VERIFY( pow(T(-4.0), 0) == T(1.0) );

    VERIFY( pow(T(1.0), 1) == T(1.0) );
    VERIFY( pow(T(2.0), 1) == T(2.0) );
    VERIFY( pow(T(-1.0), 1) == T(-1.0) );
    VERIFY( pow(T(-4.0), 1) == T(-4.0) );

    VERIFY( pow(T(1.0), -1) == T(1.0) / T(1.0) );
    VERIFY( pow(T(2.0), -1) == T(1.0) / T(2.0) );
    VERIFY( pow(T(-1.0), -1) == T(1.0) / T(-1.0) );
    VERIFY( pow(T(-4.0), -1) == T(1.0) / T(-4.0) );

    VERIFY( pow(T(1.0), 2) == T(1.0) * T(1.0) );
    VERIFY( pow(T(2.0), 2) == T(2.0) * T(2.0) );
    VERIFY( pow(T(-1.0), 2) == T(-1.0) * T(-1.0) );
    VERIFY( pow(T(-4.0), 2) == T(-4.0) * T(-4.0) );
  }

void test01()
{
  test01_do<float>();
  test01_do<double>();
  test01_do<long double>();
}

int main()
{
  test01();
  return 0;
}
