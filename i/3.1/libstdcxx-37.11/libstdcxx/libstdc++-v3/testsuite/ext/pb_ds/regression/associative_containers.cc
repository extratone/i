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
 * @file assoc_link_regression_test_1.cpp
 * A linkage regression test.
 */


#include <ext/pb_ds/assoc_container.hpp>
#include <iostream>
#include <cassert>

using namespace std;
using namespace pb_ds;

/**
 * The following function performs a sequence of operations on an
 * associative container object mapping integers to characters.
 */
template<typename Cntnr>
void
some_op_sequence(Cntnr& r_c)
{
  assert(r_c.empty());
  assert(r_c.size() == 0);

  r_c.insert(make_pair(1, 'a'));

  r_c[2] = 'b';

  assert(!r_c.empty());
  assert(r_c.size() == 2);

  cout << "Key 1 is mapped to " << r_c[1] << endl;
  cout << "Key 2 is mapped to " << r_c[2] << endl;

  cout << endl << "All value types in the container:" << endl;
  for (typename Cntnr::const_iterator it = r_c.begin(); it != r_c.end(); ++it)
    cout << it->first << " -> " << it->second << endl;

  cout << endl;

  r_c.clear();

  assert(r_c.empty());
  assert(r_c.size() == 0);
}

void
assoc_link_regression_test_0()
{
  {
    // Perform operations on a collision-chaining hash map.
    cc_hash_table<int, char> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a general-probing hash map.
    gp_hash_table<int, char> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a red-black tree map.
    tree<int, char> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a splay tree map.
    tree<int, char, less<int>, splay_tree_tag> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a splay tree map.
    tree<int, char, less<int>, ov_tree_tag> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a list-update map.
    list_update<int, char> c;
    some_op_sequence(c);
  }
}


void
assoc_link_regression_test_1()
{
  {
    // Perform operations on a collision-chaining hash map.
    cc_hash_table<int, char> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a general-probing hash map.
    gp_hash_table<int, char> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a red-black tree map.
    tree<int, char> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a splay tree map.
    tree<int, char, less<int>, splay_tree_tag> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a splay tree map.
    tree<int, char, less<int>, ov_tree_tag> c;
    some_op_sequence(c);
  }

  {
    // Perform operations on a list-update map.
    list_update<int, char> c;
    some_op_sequence(c);
  }
}

int
main()
{
  assoc_link_regression_test_0();
  assoc_link_regression_test_1();
  return 0;
}
