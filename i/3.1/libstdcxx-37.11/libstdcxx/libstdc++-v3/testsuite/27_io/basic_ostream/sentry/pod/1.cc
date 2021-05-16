// 1999-10-14 bkoz

// Copyright (C) 1999, 2001, 2003, 2005 Free Software Foundation, Inc.
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

// 27.6.1.1.2 class basic_istream::sentry

#include <ostream>
#include <sstream>
#include <ext/pod_char_traits.h>
#include <testsuite_hooks.h>
#include <testsuite_character.h>

void test01()
{
  using namespace std;
  using __gnu_test::pod_ushort;
  typedef basic_string<pod_ushort> 	string_type;
  typedef basic_stringbuf<pod_ushort> 	stringbuf_type;
  typedef basic_ostream<pod_ushort> 	ostream_type;

  bool test __attribute__((unused)) = true;


  const string_type str01;
  stringbuf_type* strbuf01 = NULL;
  stringbuf_type strbuf02(str01);
  ostream_type ostr01(strbuf01);
  ostream_type ostr02(&strbuf02);
  
  // test negatives
  try
    {
      ostream_type::sentry sentry01(ostr01);	
    }
  catch (std::bad_cast& obj)
    {
      // Not ok, throws bad_cast because locale has no ctype facet,
      // but none is needed for ostream::sentry.
      VERIFY( false );
    }
  catch (...)
    {
      VERIFY( false );
    }

  // imbued.
  const std::locale loc(std::locale::classic(), new std::ctype<pod_ushort>);
  ostr01.imbue(loc);
  try
    {
      ostream_type::sentry sentry01(ostr01);	
      VERIFY( bool(sentry01) == false ); 
    }
  catch (...)
    {
      VERIFY( false );
    }

  // test positive 
  try
    {
      ostream_type::sentry sentry03(ostr02);	
    }
  catch (std::bad_cast& obj)
    {
      // Not ok, throws bad_cast because locale has no ctype facet,
      // but none is needed for ostream::sentry.
      VERIFY( false );
    }
  catch (...)
    {
      VERIFY( false );
    }

  // imbued.
  ostr02.clear();
  ostr02.imbue(loc);
  try
    {
      ostream_type::sentry sentry03(ostr02);	
      VERIFY( bool(sentry03) == true ); 
    }
  catch (...)
    {
      VERIFY( false );
    }
}

#if !__GXX_WEAK__
// Explicitly instantiate for systems with no COMDAT or weak support.
template 
  std::basic_string<__gnu_test::pod_ushort>::size_type 
  std::basic_string<__gnu_test::pod_ushort>::_Rep::_S_max_size;

template 
  __gnu_test::pod_ushort
  std::basic_string<__gnu_test::pod_ushort>::_Rep::_S_terminal;
#endif

int main() 
{
  test01();
  return 0;
}
