// Copyright (C) 2005 Free Software Foundation, Inc.
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

#include <ostream>
#include <streambuf>
#include <testsuite_hooks.h>

struct buf: std::wstreambuf
{
  virtual int_type overflow(int_type) 
  { throw 0; }
};

template<typename T>
void testthrow(T arg)
{
  bool test __attribute__((unused)) = true;
  buf b;
  std::wostream os(&b);
  os.exceptions(std::wios::badbit);

  try 
    {
      os << arg;
    }
  catch(int)
    {
      // Expected return is zero.
      VERIFY( os.bad() );
    }
  catch(...) 
    {
      VERIFY( false );
    }    
}

int main()
{
  bool b = true;
  short s = -4; 
  unsigned short us = 4;
  int i = -45; 
  unsigned int ui = 45;
  long l = -456;
  unsigned long ul = 456;
  float f = 3.4;
  double d = 3.45;
  long double ld = 3.456;

  testthrow(b);
  testthrow(s);
  testthrow(us);
  testthrow(i);
  testthrow(ui);
  testthrow(l);
  testthrow(ul);
  testthrow(f);
  testthrow(d);
  testthrow(ld);

  return 0;
}
