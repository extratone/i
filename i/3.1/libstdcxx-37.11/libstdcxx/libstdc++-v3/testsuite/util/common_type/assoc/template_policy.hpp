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
 * @file template_policy.hpp
 * Contains template versions of policies.
 */

#ifndef PB_DS_TEMPLATE_POLICY_HPP
#define PB_DS_TEMPLATE_POLICY_HPP

#include <ext/typelist.h>
#include <ext/pb_ds/hash_policy.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#include <ext/pb_ds/list_update_policy.hpp>

namespace pb_ds
{
  namespace test
  {
    template<typename Allocator>
    struct direct_mask_range_hashing_t_ 
    : public pb_ds::direct_mask_range_hashing<typename Allocator::size_type>
    {
      typedef typename Allocator::size_type size_type;
      typedef pb_ds::direct_mask_range_hashing<size_type> base_type;
    };

    template<typename Allocator>
    struct direct_mod_range_hashing_t_ 
    : public pb_ds::direct_mod_range_hashing<typename Allocator::size_type>
    {
      typedef typename Allocator::size_type size_type;
      typedef pb_ds::direct_mod_range_hashing<size_type> base_type;
    };

    template<typename Allocator,
	     typename Allocator::size_type Min_Load_Nom,
	     typename Allocator::size_type Min_Load_Denom,
	     typename Allocator::size_type Max_Load_Nom,
	     typename Allocator::size_type Max_Load_Denom,
	     bool External_Access>
    struct hash_load_check_resize_trigger_t_ 
    : public pb_ds::hash_load_check_resize_trigger<External_Access,
						   typename Allocator::size_type>
    {
      typedef typename Allocator::size_type size_type;
      typedef pb_ds::hash_load_check_resize_trigger<External_Access, size_type>  base_type;

      inline
      hash_load_check_resize_trigger_t_() 
      : base_type(static_cast<float>(Min_Load_Nom) / static_cast<float>(Min_Load_Denom), static_cast<float>(Max_Load_Nom) / static_cast<float>(Max_Load_Denom))
      { }

      enum
	{
	  get_set_loads = External_Access,
	  get_set_load = false
	};
    };

    template<typename Allocator,
	     typename Allocator::size_type Load_Nom,
	     typename Allocator::size_type Load_Denom,
	     bool External_Access>
    struct cc_hash_max_collision_check_resize_trigger_t_ 
    : public pb_ds::cc_hash_max_collision_check_resize_trigger<External_Access,
      typename Allocator::size_type>
    {
      typedef typename Allocator::size_type size_type;
      typedef pb_ds::cc_hash_max_collision_check_resize_trigger<External_Access, size_type> base_type;

      inline
      cc_hash_max_collision_check_resize_trigger_t_() 
      : base_type(static_cast<float>(Load_Nom) / static_cast<float>(Load_Denom))
      { }

      enum
	{
	  get_set_load = External_Access,
	  get_set_loads = false
	};
    };

    struct hash_prime_size_policy_t_ : public pb_ds::hash_prime_size_policy
    { };

    template<typename Allocator>
    struct hash_exponential_size_policy_t_ 
    : public pb_ds::hash_exponential_size_policy<typename Allocator::size_type>
    { };

    template<typename Key, class Allocator>
    struct linear_probe_fn_t_ 
    : public pb_ds::linear_probe_fn<typename Allocator::size_type>
    { };

    template<typename Key, class Allocator>
    struct quadratic_probe_fn_t_ 
    : public pb_ds::quadratic_probe_fn<typename Allocator::size_type>
    { };

    template<typename Allocator, typename Allocator::size_type Max_Count>
    struct counter_lu_policy_t_ 
    : public pb_ds::counter_lu_policy<Max_Count, Allocator>
    {
      typedef pb_ds::counter_lu_policy<Max_Count, Allocator> base_type;
    };

    struct move_to_front_lu_policy_t_ 
    : public pb_ds::move_to_front_lu_policy<>
    { };
  } // namespace test
} // namespace pb_ds

#endif 

