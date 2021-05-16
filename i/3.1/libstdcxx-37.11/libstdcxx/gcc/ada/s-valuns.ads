------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                       S Y S T E M . V A L _ U N S                        --
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

--  This package contains routines for scanning modular Unsigned
--  values for use in Text_IO.Modular_IO, and the Value attribute.

with System.Unsigned_Types;

package System.Val_Uns is
   pragma Pure;

   function Scan_Raw_Unsigned
     (Str : String;
      Ptr : access Integer;
      Max : Integer) return System.Unsigned_Types.Unsigned;
   --  This function scans the string starting at Str (Ptr.all) for a valid
   --  integer according to the syntax described in (RM 3.5(43)). The substring
   --  scanned extends no further than Str (Max).  Note: this does not scan
   --  leading or trailing blanks, nor leading sign.
   --
   --  There are three cases for the return:
   --
   --  If a valid integer is found, then Ptr.all is updated past the last
   --  character of the integer.
   --
   --  If no valid integer is found, then Ptr.all points either to an initial
   --  non-digit character, or to Max + 1 if the field is all spaces and the
   --  exception Constraint_Error is raised.
   --
   --  If a syntactically valid integer is scanned, but the value is out of
   --  range, or, in the based case, the base value is out of range or there
   --  is an out of range digit, then Ptr.all points past the integer, and
   --  Constraint_Error is raised.
   --
   --  Note: these rules correspond to the requirements for leaving the pointer
   --  positioned in Text_IO.Get
   --
   --  Note: if Str is empty, i.e. if Max is less than Ptr, then this is a
   --  special case of an all-blank string, and Ptr is unchanged, and hence
   --  is greater than Max as required in this case.

   function Scan_Unsigned
     (Str : String;
      Ptr : access Integer;
      Max : Integer) return System.Unsigned_Types.Unsigned;
   --  Same as Scan_Raw_Unsigned, except scans optional leading
   --  blanks, and an optional leading plus sign.
   --  Note: if a minus sign is present, Constraint_Error will be raised.
   --  Note: trailing blanks are not scanned.

   function Value_Unsigned
     (Str : String) return System.Unsigned_Types.Unsigned;
   --  Used in computing X'Value (Str) where X is a modular integer type whose
   --  modulus does not exceed the range of System.Unsigned_Types.Unsigned. Str
   --  is the string argument of the attribute. Constraint_Error is raised if
   --  the string is malformed, or if the value is out of range.

end System.Val_Uns;
