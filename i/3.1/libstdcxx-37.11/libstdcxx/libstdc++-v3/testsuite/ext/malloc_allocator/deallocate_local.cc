//
// Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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

// 20.4.1.1 allocator members

#include <string>
#include <ext/malloc_allocator.h>
#include <testsuite_hooks.h>

static size_t alloc_cnt;
 
void* operator new(size_t size) throw(std::bad_alloc)
{
  printf("operator new is called \n");
  void* p = malloc(size);
  if (p == NULL)
    throw std::bad_alloc();
  alloc_cnt++;
  return p;
}
 
void operator delete(void* p) throw()
{
  printf("operator delete is called \n");
  if (p == NULL)
    return;
  alloc_cnt--;
  if (alloc_cnt == 0)
    printf("All memory released \n");
  else
    printf("%lu allocations to be released \n",
	   static_cast<unsigned long>(alloc_cnt));
  free(p);
}

typedef char char_t;
typedef std::char_traits<char_t> traits_t;
typedef __gnu_cxx::malloc_allocator<char_t> allocator_t;
typedef std::basic_string<char_t, traits_t, allocator_t> string_t;

int main()
{
  bool test __attribute__((unused)) = true;
  {
    string_t s;
    s += "bayou bend";
  }
  VERIFY( alloc_cnt == 0 );
  return 0;
}
