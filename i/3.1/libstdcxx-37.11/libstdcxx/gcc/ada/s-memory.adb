------------------------------------------------------------------------------
--                                                                          --
--                         GNAT RUN-TIME COMPONENTS                         --
--                                                                          --
--                         S Y S T E M . M E M O R Y                        --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--          Copyright (C) 2001-2005 Free Software Foundation, Inc.          --
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

--  This is the default implementation of this package.

--  This implementation assumes that the underlying malloc/free/realloc
--  implementation is thread safe, and thus, no additional lock is required.
--  Note that we still need to defer abort because on most systems, an
--  asynchronous signal (as used for implementing asynchronous abort of
--  task) cannot safely be handled while malloc is executing.

--  If you are not using Ada constructs containing the "abort" keyword, then
--  you can remove the calls to Abort_Defer.all and Abort_Undefer.all from
--  this unit.

with Ada.Exceptions;
with System.Soft_Links;
with System.Parameters;
with System.CRTL;

package body System.Memory is

   use Ada.Exceptions;
   use System.Soft_Links;

   function c_malloc (Size : System.CRTL.size_t) return System.Address
    renames System.CRTL.malloc;

   procedure c_free (Ptr : System.Address)
     renames System.CRTL.free;

   function c_realloc
     (Ptr : System.Address; Size : System.CRTL.size_t) return System.Address
     renames System.CRTL.realloc;

   -----------
   -- Alloc --
   -----------

   function Alloc (Size : size_t) return System.Address is
      Result      : System.Address;
      Actual_Size : size_t := Size;

   begin
      if Size = size_t'Last then
         Raise_Exception (Storage_Error'Identity, "object too large");
      end if;

      --  Change size from zero to non-zero. We still want a proper pointer
      --  for the zero case because pointers to zero length objects have to
      --  be distinct, but we can't just go ahead and allocate zero bytes,
      --  since some malloc's return zero for a zero argument.

      if Size = 0 then
         Actual_Size := 1;
      end if;

      if Parameters.No_Abort then
         Result := c_malloc (System.CRTL.size_t (Actual_Size));
      else
         Abort_Defer.all;
         Result := c_malloc (System.CRTL.size_t (Actual_Size));
         Abort_Undefer.all;
      end if;

      if Result = System.Null_Address then
         Raise_Exception (Storage_Error'Identity, "heap exhausted");
      end if;

      return Result;
   end Alloc;

   ----------
   -- Free --
   ----------

   procedure Free (Ptr : System.Address) is
   begin
      if Parameters.No_Abort then
         c_free (Ptr);
      else
         Abort_Defer.all;
         c_free (Ptr);
         Abort_Undefer.all;
      end if;
   end Free;

   -------------
   -- Realloc --
   -------------

   function Realloc
     (Ptr  : System.Address;
      Size : size_t)
      return System.Address
   is
      Result      : System.Address;
      Actual_Size : constant size_t := Size;

   begin
      if Size = size_t'Last then
         Raise_Exception (Storage_Error'Identity, "object too large");
      end if;

      if Parameters.No_Abort then
         Result := c_realloc (Ptr, System.CRTL.size_t (Actual_Size));
      else
         Abort_Defer.all;
         Result := c_realloc (Ptr, System.CRTL.size_t (Actual_Size));
         Abort_Undefer.all;
      end if;

      if Result = System.Null_Address then
         Raise_Exception (Storage_Error'Identity, "heap exhausted");
      end if;

      return Result;
   end Realloc;

end System.Memory;
