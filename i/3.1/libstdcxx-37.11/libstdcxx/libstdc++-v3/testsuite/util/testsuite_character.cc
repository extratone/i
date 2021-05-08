// -*- C++ -*-

// Utility subroutines for the C++ library testsuite. 
//
// Copyright (C) 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
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
//
// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

#include <testsuite_character.h>

namespace std
{
  locale::id
  codecvt<__gnu_test::pod_uchar, char, __gnu_test::pod_state>::id;

  locale::id 
  ctype<__gnu_test::pod_uchar>::id;

  locale::id
  numpunct<__gnu_test::pod_uint>::id;

  locale::id
  moneypunct<__gnu_test::pod_uint>::id;

  // Member specializations for the existing facet classes.  
  // NB: This isn't especially portable. Perhaps a better way would be
  // to just specialize all of numpunct and ctype.
  using __gnu_test::pod_ushort;
  typedef pod_ushort::value_type value_type;

  template<>
    bool 
    ctype<pod_ushort>::
    do_is(mask, char_type) const { return true; }

  template<>
    const pod_ushort*
    ctype<pod_ushort>::
    do_is(const char_type* __lo, const char_type*, mask*) const
    { return __lo; }

  template<>
    const pod_ushort*
    ctype<pod_ushort>::
    do_scan_is(mask, const char_type* __lo, const char_type*) const
    { return __lo; }

  template<>
    const pod_ushort*
    ctype<pod_ushort>::
    do_scan_not(mask, const char_type* __lo, const char_type*) const
    { return __lo; }

  template<>
    pod_ushort 
    ctype<pod_ushort>::
    do_toupper(char_type __c) const
    { return __c; }

  template<>
    const pod_ushort*
    ctype<pod_ushort>::
    do_toupper(char_type*, const char_type* __hi) const
    { return __hi; }

  template<>
    pod_ushort 
    ctype<pod_ushort>::
    do_tolower(char_type __c) const
    { return __c; }

  template<>
    const pod_ushort*
    ctype<pod_ushort>::
    do_tolower(char_type*, const char_type* __hi) const
    { return __hi; }

  template<>
    pod_ushort
    ctype<pod_ushort>::
    do_widen(char __c) const
    { 
      char_type ret = { value_type(__c) };
      return ret;
    }

  template<>
    const char*
    ctype<pod_ushort>::
    do_widen(const char* __lo, const char* __hi, char_type* __dest) const
    {
      while (__lo < __hi)
	{
	  *__dest = this->do_widen(*__lo);
	  ++__lo;
	  ++__dest;
	}
      return __hi;
    }

  template<>
    char
    ctype<pod_ushort>::
    do_narrow(char_type __wc, char) const
    { return static_cast<char>(__wc.value); }

  template<>
    const pod_ushort*
    ctype<pod_ushort>::
    do_narrow(const pod_ushort* __lo, const pod_ushort* __hi, 
	      char, char* __dest) const
    {
      while (__lo < __hi)
	{
	  *__dest = this->do_narrow(*__lo, char());
	  ++__lo;
	  ++__dest;
	}
      return __hi;
    }

  template<>
    ctype<pod_ushort>::~ctype() { }

  template<>
    void
    numpunct<pod_ushort>::_M_initialize_numpunct(__c_locale)
    {
      if (!_M_data)
	_M_data = new __numpunct_cache<pod_ushort>;

      _M_data->_M_grouping = "";
      _M_data->_M_use_grouping = false;

      _M_data->_M_decimal_point.value =  value_type('.');
      _M_data->_M_thousands_sep.value = value_type(',');
      
      for (size_t i = 0; i < __num_base::_S_oend; ++i)
	{
	  value_type v = __num_base::_S_atoms_out[i];
	  _M_data->_M_atoms_out[i].value = v;
	}
      _M_data->_M_atoms_out[__num_base::_S_oend] = pod_ushort();
      
      for (size_t j = 0; j < __num_base::_S_iend; ++j)
	_M_data->_M_atoms_in[j].value = value_type(__num_base::_S_atoms_in[j]);
      _M_data->_M_atoms_in[__num_base::_S_iend] = pod_ushort();

      // "true"
      pod_ushort* __truename = new pod_ushort[4 + 1];
      __truename[0].value = value_type('t');
      __truename[1].value = value_type('r');
      __truename[2].value = value_type('u');
      __truename[3].value = value_type('e');
      __truename[4] = pod_ushort();
      _M_data->_M_truename = __truename;

      // "false"
      pod_ushort* __falsename = new pod_ushort[5 + 1];
      __falsename[0].value = value_type('f');
      __falsename[1].value = value_type('a');
      __falsename[2].value = value_type('l');
      __falsename[3].value = value_type('s');
      __falsename[4].value = value_type('e');
      __falsename[5] = pod_ushort();
      _M_data->_M_falsename = __falsename;
    }

  template<>
    numpunct<pod_ushort>::~numpunct()
    { delete _M_data; }
} // namespace std
