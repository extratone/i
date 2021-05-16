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
 * @file priority_queue_erase_if.cpp
 * Example showing how to conditionally erase values from a priority queue.
 */

/**
 * This example shows how to erase from a priority queue using erase_if.
 */

#include <functional>
#include <iostream>
#include <cassert>
#include <ext/pb_ds/priority_queue.hpp>

using namespace std;
using namespace pb_ds;
using namespace pb_ds;

int
main()
{
  pb_ds::priority_queue<int> p;

  // First we insert some values into the container.
  for (int i = 0; i < 1000; ++i)
    p.push(i);

  assert(p.top() == 999);

  // Now we erase all values that satisfy some predicate, in this case
  // one that returns true for all those larger than 500.
  p.erase_if(bind1st(less<int>(), 500));

  // The largest value should be now 500.
  assert(p.top() == 500);
}

