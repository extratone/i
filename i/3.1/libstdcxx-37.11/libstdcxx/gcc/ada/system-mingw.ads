------------------------------------------------------------------------------
--                                                                          --
--                        GNAT RUN-TIME COMPONENTS                          --
--                                                                          --
--                               S Y S T E M                                --
--                                                                          --
--                                 S p e c                                  --
--                            (Windows Version)                             --
--                                                                          --
--          Copyright (C) 1992-2006, Free Software Foundation, Inc.         --
--                                                                          --
-- This specification is derived from the Ada Reference Manual for use with --
-- GNAT. The copyright notice above, and the license provisions that follow --
-- apply solely to the  contents of the part following the private keyword. --
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

package System is
   pragma Pure;
   --  Note that we take advantage of the implementation permission to make
   --  this unit Pure instead of Preelaborable; see RM 13.7.1(15). In Ada
   --  2005, this is Pure in any case (AI-362).

   type Name is (SYSTEM_NAME_GNAT);
   System_Name : constant Name := SYSTEM_NAME_GNAT;

   --  System-Dependent Named Numbers

   Min_Int               : constant := Long_Long_Integer'First;
   Max_Int               : constant := Long_Long_Integer'Last;

   Max_Binary_Modulus    : constant := 2 ** Long_Long_Integer'Size;
   Max_Nonbinary_Modulus : constant := Integer'Last;

   Max_Base_Digits       : constant := Long_Long_Float'Digits;
   Max_Digits            : constant := Long_Long_Float'Digits;

   Max_Mantissa          : constant := 63;
   Fine_Delta            : constant := 2.0 ** (-Max_Mantissa);

   Tick                  : constant := 0.01;

   --  Storage-related Declarations

   type Address is private;
   Null_Address : constant Address;

   Storage_Unit : constant := 8;
   Word_Size    : constant := 32;
   Memory_Size  : constant := 2 ** 32;

   --  Address comparison

   function "<"  (Left, Right : Address) return Boolean;
   function "<=" (Left, Right : Address) return Boolean;
   function ">"  (Left, Right : Address) return Boolean;
   function ">=" (Left, Right : Address) return Boolean;
   function "="  (Left, Right : Address) return Boolean;

   pragma Import (Intrinsic, "<");
   pragma Import (Intrinsic, "<=");
   pragma Import (Intrinsic, ">");
   pragma Import (Intrinsic, ">=");
   pragma Import (Intrinsic, "=");

   --  Other System-Dependent Declarations

   type Bit_Order is (High_Order_First, Low_Order_First);
   Default_Bit_Order : constant Bit_Order := Low_Order_First;

   --  Priority-related Declarations (RM D.1)

   Max_Priority           : constant Positive := 30;
   Max_Interrupt_Priority : constant Positive := 31;

   subtype Any_Priority       is Integer      range  0 .. 31;
   subtype Priority           is Any_Priority range  0 .. 30;
   subtype Interrupt_Priority is Any_Priority range 31 .. 31;

   Default_Priority : constant Priority := 15;

private

   type Address is mod Memory_Size;
   Null_Address : constant Address := 0;

   --------------------------------------
   -- System Implementation Parameters --
   --------------------------------------

   --  These parameters provide information about the target that is used
   --  by the compiler. They are in the private part of System, where they
   --  can be accessed using the special circuitry in the Targparm unit
   --  whose source should be consulted for more detailed descriptions
   --  of the individual switch values.

   AAMP                      : constant Boolean := False;
   Backend_Divide_Checks     : constant Boolean := False;
   Backend_Overflow_Checks   : constant Boolean := False;
   Command_Line_Args         : constant Boolean := True;
   Compiler_System_Version   : constant Boolean := False;
   Configurable_Run_Time     : constant Boolean := False;
   Denorm                    : constant Boolean := True;
   Duration_32_Bits          : constant Boolean := False;
   Exit_Status_Supported     : constant Boolean := True;
   Fractional_Fixed_Ops      : constant Boolean := False;
   Frontend_Layout           : constant Boolean := False;
   Functions_Return_By_DSP   : constant Boolean := False;
   Machine_Overflows         : constant Boolean := False;
   Machine_Rounds            : constant Boolean := True;
   OpenVMS                   : constant Boolean := False;
   Preallocated_Stacks       : constant Boolean := False;
   Signed_Zeros              : constant Boolean := True;
   Stack_Check_Default       : constant Boolean := False;
   Stack_Check_Probes        : constant Boolean := True;
   Support_64_Bit_Divides    : constant Boolean := True;
   Support_Aggregates        : constant Boolean := True;
   Support_Composite_Assign  : constant Boolean := True;
   Support_Composite_Compare : constant Boolean := True;
   Support_Long_Shifts       : constant Boolean := True;
   Suppress_Standard_Library : constant Boolean := False;
   Use_Ada_Main_Program_Name : constant Boolean := False;
   ZCX_By_Default            : constant Boolean := False;
   GCC_ZCX_Support           : constant Boolean := True;
   Front_End_ZCX_Support     : constant Boolean := False;

   --  Obsolete entries, to be removed eventually (bootstrap issues!)

   High_Integrity_Mode       : constant Boolean := False;
   Long_Shifts_Inlined       : constant Boolean := False;

   ---------------------------
   -- Underlying Priorities --
   ---------------------------

   --  Important note: this section of the file must come AFTER the
   --  definition of the system implementation parameters to ensure
   --  that the value of these parameters is available for analysis
   --  of the declarations here (using Rtsfind at compile time).

   --  The underlying priorities table provides a generalized mechanism
   --  for mapping from Ada priorities to system priorities. In some
   --  cases a 1-1 mapping is not the convenient or optimal choice.

   type Priorities_Mapping is array (Any_Priority) of Integer;
   pragma Suppress_Initialization (Priorities_Mapping);
   --  Suppress initialization in case gnat.adc specifies Normalize_Scalars

   Underlying_Priorities : constant Priorities_Mapping :=
     (Priority'First ..
      Default_Priority - 8    => -15,
      Default_Priority - 7    => -7,
      Default_Priority - 6    => -6,
      Default_Priority - 5    => -5,
      Default_Priority - 4    => -4,
      Default_Priority - 3    => -3,
      Default_Priority - 2    => -2,
      Default_Priority - 1    => -1,
      Default_Priority        => 0,
      Default_Priority + 1    => 1,
      Default_Priority + 2    => 2,
      Default_Priority + 3    => 3,
      Default_Priority + 4    => 4,
      Default_Priority + 5    => 5,
      Default_Priority + 6 ..
      Priority'Last           => 6,
      Interrupt_Priority      => 15);
   --  The default mapping preserves the standard 31 priorities of the Ada
   --  model, but maps them using compression onto the 7 priority levels
   --  available in NT and on the 16 priority levels available in 2000/XP.

   --  To replace the default values of the Underlying_Priorities mapping,
   --  copy this source file into your build directory, edit the file to
   --  reflect your desired behavior, and recompile using Makefile.adalib
   --  which can be found under the adalib directory of your gnat installation

   pragma Linker_Options ("-Wl,--stack=0x2000000");
   --  This is used to change the default stack (32 MB) size for non tasking
   --  programs. We change this value for GNAT on Windows here because the
   --  binutils on this platform have switched to a too low value for Ada
   --  programs. Note that we also set the stack size for tasking programs in
   --  System.Task_Primitives.Operations.

end System;
