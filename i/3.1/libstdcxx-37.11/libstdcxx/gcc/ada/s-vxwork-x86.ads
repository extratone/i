------------------------------------------------------------------------------
--                                                                          --
--                  GNAT RUN-TIME LIBRARY (GNARL) COMPONENTS                --
--                                                                          --
--                        S Y S T E M . V X W O R K S                       --
--                                                                          --
--                                   S p e c                                --
--                                                                          --
--            Copyright (C) 1998-2005 Free Software Foundation, Inc.        --
--                                                                          --
-- GNARL is free software; you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 2,  or (at your option) any later ver- --
-- sion. GNARL is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNARL; see file COPYING.  If not, write --
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
-- GNARL was developed by the GNARL team at Florida State University.       --
-- Extensive contributions were provided by Ada Core Technologies, Inc.     --
--                                                                          --
------------------------------------------------------------------------------

--  This is the x86 VxWorks version of this package

package System.VxWorks is
   pragma Preelaborate;

   --  Floating point context record. x86 version

   --  There are two kinds of FP_CONTEXT for this architecture, corresponding
   --  to newer and older processors. The type is defined in fppI86lib.h as a
   --  union. The form used depends on the versions of the save and restore
   --  routines that are selected by the user (these versions are provided in
   --  vxwork.ads). Since we do not examine the contents of these objects, it
   --  is sufficient to declare the type as of the required size: 512 bytes.

   type FP_CONTEXT is array (1 .. 128) of Integer;
   for FP_CONTEXT'Alignment use 4;
   for FP_CONTEXT'Size use 512 * Storage_Unit;
   pragma Convention (C, FP_CONTEXT);

   Num_HW_Interrupts : constant := 256;
   --  Number of entries in hardware interrupt vector table

end System.VxWorks;
