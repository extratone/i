------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                 S Y S T E M . C H E C K E D _ P O O L S                  --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--          Copyright (C) 1992-2006, Free Software Foundation, Inc.         --
--                                                                          --
-- GNAT is free software;  you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 2,  or (at your option) any later ver- --
-- sion.  GNAT is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNAT;  see file COPYING.  If not, write --
-- to  the  Free Software Foundation,  51  Franklin  Street,  Fifth  Floor, --
-- Boston, MA 02110-1301, USA.                                              --
--                                                                          --
-- As a special exception,  if other files  instantiate  generics from this --
-- unit, or you link  this unit with other files  to produce an executable, --
-- this  unit  does not  by itself cause  the resulting  executable  to  be --
-- covered  by the  GNU  General  Public  License.  This exception does not --
-- however invalidate  any other reasons why  the executable file  might be --
-- covered by the  GNU Public License.                                      --
--                                                                          --
-- GNAT was originally developed  by the GNAT team at  New York University. --
-- Extensive contributions were provided by Ada Core Technologies Inc.      --
--                                                                          --
------------------------------------------------------------------------------

with System.Storage_Elements;
with System.Storage_Pools;
package System.Checked_Pools is

   type Checked_Pool is abstract
     new System.Storage_Pools.Root_Storage_Pool with private;
   --  Equivalent of storage pools with the addition that Dereference is
   --  called on each implicit or explicit dereference of a pointer which
   --  has such a storage pool

   procedure Dereference
     (Pool                     : in out Checked_Pool;
      Storage_Address          : Address;
      Size_In_Storage_Elements : System.Storage_Elements.Storage_Count;
      Alignment                : System.Storage_Elements.Storage_Count)
   is abstract;
   --  Called implicitly each time a pointer to a checked pool is dereferenced
   --  All parameters in the profile are compatible with the profile of
   --  Allocate/Deallocate: the Storage_Address corresponds to the address of
   --  the dereferenced object, Size_in_Storage_Elements is its dynamic size
   --  (and thus may involve an implicit dispatching call to size) and
   --  Alignment is the alignment of the object.

private
   type Checked_Pool is abstract
     new System.Storage_Pools.Root_Storage_Pool with null record;
end System.Checked_Pools;
