// -*- C++ -*-

// Copyright (C) 2005, 2006 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the terms
// of the GNU General Public License as published by the Free Software
// Foundation; either version 2, or (at your option) any later
// version.

// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
// MA 02111-1307, USA.

// As a special exception, you may use this file as part of a free
// software library without restriction.  Specifically, if other files
// instantiate templates or use macros or inline functions from this
// file, or you compile this file and link it with other files to
// produce an executable, this file does not by itself cause the
// resulting executable to be covered by the GNU General Public
// License.  This exception does not however invalidate any other
// reasons why the executable file might be covered by the GNU General
// Public License.

// Copyright (C) 2004 Ami Tavory and Vladimir Dreizin, IBM-HRL.

// Permission to use, copy, modify, sell, and distribute this software
// is hereby granted without fee, provided that the above copyright
// notice appears in all copies, and that both that copyright notice
// and this permission notice appear in supporting documentation. None
// of the above authors, nor IBM Haifa Research Laboratories, make any
// representation about the suitability of this software for any
// purpose. It is provided "as is" without express or implied
// warranty.

/**
 * @file multimap_common_type.hpp
 * Contains types for a generic multimap_insert_test test.
 */

#ifndef PB_DS_MULTIMAP_RANDOM_INT_INSERT_TEST_COMMON_TYPE_HPP
#define PB_DS_MULTIMAP_RANDOM_INT_INSERT_TEST_COMMON_TYPE_HPP

#include <hash_fn/string_hash_fn.hpp>
#include <common_type/assoc/common_type.hpp>

namespace pb_ds
{
  namespace test
  {
    namespace detail
    {
      struct int_hash : public std::unary_function<int, size_t>
      {
	inline size_t
        operator()(const int i) const
	{ return (static_cast<size_t>(i)); }
      };

      template<typename Key, typename Allocator = std::allocator<char> >
      struct hash_set_tl_t
      {
	typedef
        typename pb_ds::test::hash_common_types<
	  Key,
	  pb_ds::null_mapped_type,
	  int_hash,
	  std::equal_to<Key>,
	  Allocator>::performance_min_tl
        type;
      };

      template<typename Key, typename Allocator = std::allocator<char> >
      struct lu_set_tl_t
      {
	typedef
        typename pb_ds::test::lu_common_types<
	  Key,
	  pb_ds::null_mapped_type,
	  std::equal_to<
	  Key>,
	  Allocator>::performance_min_tl
        type;
      };

      template<typename Key,
	       class Sec_Tl,
	       typename Allocator = std::allocator<char> >
      struct hash_mmap_tl_t
      {
      private:
	typedef
        typename pb_ds::detail::__conditional_type<
	pb_ds::detail::is_same<
	int,
	Key>::value,
	int_hash,
	string_hash_fn>::__type
        hash_fn_t;

	template<typename Cntnr_T>
	struct hash_mmap_transform
	{
	  typedef
	  typename pb_ds::test::hash_common_types<
	    Key,
	    Cntnr_T,
	    hash_fn_t,
	    std::equal_to<
	    Key>,
	    Allocator>::performance_min_tl
	  type;
	};

      public:
	typedef
        typename __gnu_cxx::typelist::flatten<
	typename __gnu_cxx::typelist::transform<
	Sec_Tl,
	hash_mmap_transform>::type>::type
        type;
      };

      template<typename Key,
	       class Sec_Tl,
	       typename Allocator = std::allocator<char> >
      struct tree_mmap_tl_t
      {
      private:
	template<typename Cntnr_T>
	struct tree_mmap_transform
	{
	  typedef
	  typename pb_ds::test::tree_common_types<
	    Key,
	    Cntnr_T,
	    std::less<
	    Key>,
	    pb_ds::null_tree_node_update,
	    Allocator>::performance_min_tl
	  type;
	};

      public:
	typedef
        typename __gnu_cxx::typelist::flatten<
	typename __gnu_cxx::typelist::transform<
	Sec_Tl,
	tree_mmap_transform>::type>::type
        type;
      };

      template<typename Key, typename Mapped, typename Allocator>
      struct hash_hash_mmap_tl_t
      {
      private:
	typedef typename hash_set_tl_t<Mapped, Allocator>::type sec_tl_t;

      public:
	typedef typename hash_mmap_tl_t<Key, sec_tl_t, Allocator>::type type;
      };

      template<typename Key, typename Mapped, typename Allocator>
      struct tree_hash_mmap_tl_t
      {
      private:
	typedef typename hash_set_tl_t<Mapped, Allocator>::type sec_tl_t;

      public:
	typedef typename tree_mmap_tl_t<Key, sec_tl_t, Allocator>::type type;
      };

      template<typename Key, typename Mapped, typename Allocator>
      struct tree_lu_mmap_tl_t
      {
      private:
	typedef typename lu_set_tl_t<Mapped, Allocator>::type sec_tl_t;

      public:
	typedef typename tree_mmap_tl_t<Key, sec_tl_t, Allocator>::type type;
      };

      template<typename Key, typename Mapped, typename Allocator>
      struct hash_lu_mmap_tl_t
      {
      private:
	typedef typename lu_set_tl_t<Mapped, Allocator>::type sec_tl_t;

      public:
	typedef typename hash_mmap_tl_t<Key, sec_tl_t, Allocator>::type type;
      };
    } // namespace detail

    template<typename Key, typename Mapped, typename Allocator>
    struct mmap_tl_t
    {
    private:
      typedef
      typename detail::hash_hash_mmap_tl_t<Key, Mapped, Allocator>::type
      hh_mmap_tl_t;

      typedef
      typename detail::hash_lu_mmap_tl_t<Key, Mapped, Allocator>::type
      hl_mmap_tl_t;

      typedef
      typename detail::tree_hash_mmap_tl_t<Key, Mapped, Allocator>::type
      th_mmap_tl_t;

      typedef
      typename detail::tree_lu_mmap_tl_t<Key, Mapped, Allocator>::type
      tl_mmap_tl_t;

    public:
      typedef
      typename __gnu_cxx::typelist::append<hl_mmap_tl_t,
      typename __gnu_cxx::typelist::append<hh_mmap_tl_t,
      typename __gnu_cxx::typelist::append<th_mmap_tl_t,
      tl_mmap_tl_t>::type>::type>::type
      type;
    };

  } // namespace test
} // namespace pb_ds

#endif

