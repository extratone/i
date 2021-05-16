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
 * @file sample_mean_confidence_checker.hpp
 * Contains a function for checking the confidence of a sample mean
 */

#ifndef PB_DS_SAMPLE_MEAN_CONFIDENCE_CHECKER_HPP
#define PB_DS_SAMPLE_MEAN_CONFIDENCE_CHECKER_HPP

#include <list>
#include <numeric>
#include <math.h>

namespace pb_ds
{
  namespace test
  {
    namespace detail
    {
      /*
       * Checks that a sample mean sm is in the relative interval
       * relative_interval of a true mean (assuming i.i.d. samples),
       * given a sample variance sv taken over num_samples samples,
       * with confidence ~ 0.95.
       *
       * See "Probability, Random Variables, and Stochastic Processes"
       * (Third edition) Athanasios Papoulis, Chapter 9.
       */
      template<typename Value_Type>
      bool
      sample_mean_confidence_checker(Value_Type sm, Value_Type sv, 
				     std::size_t num_samples, 
				     double relative_interval)
      {
	enum
	  {
	    // Ensures that the student-t distribution is approximately normal.
	    min_num_samples = 30
	  };

	if (num_samples < min_num_samples)
	  return (false);

	// This is z_u (normal-dist percentile) for u = 0.975.
	const Value_Type z = 1.976;

	return (sv / ::sqrt(double(num_samples)) <= relative_interval * sm / z);
      }
    } // namespace detail
  } // namespace test
} // namespace pb_ds

#endif 

