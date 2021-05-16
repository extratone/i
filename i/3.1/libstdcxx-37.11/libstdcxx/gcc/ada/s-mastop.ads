------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                      SYSTEM.MACHINE_STATE_OPERATIONS                     --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--          Copyright (C) 1999-2005 Free Software Foundation, Inc.          --
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

pragma Polling (Off);
--  We must turn polling off for this unit, because otherwise we get
--  elaboration circularities with System.Exception_Tables.

with System.Storage_Elements;

package System.Machine_State_Operations is

   subtype Code_Loc is System.Address;
   --  Code location used in building exception tables and for call
   --  addresses when propagating an exception (also traceback table)
   --  Values of this type are created by using Label'Address or
   --  extracted from machine states using Get_Code_Loc.

   type Machine_State is new System.Address;
   --  The table based exception handling approach (see a-except.adb) isolates
   --  the target dependent aspects using an abstract data type interface
   --  to the type Machine_State, which is represented as a System.Address
   --  value (presumably implemented as a pointer to an appropriate record
   --  structure).

   function Machine_State_Length return System.Storage_Elements.Storage_Offset;
   --  Function to determine the length of the Storage_Array needed to hold
   --  a machine state. The machine state will always be maximally aligned.
   --  The value returned is a constant that will be used to allocate space
   --  for a machine state value.

   function Allocate_Machine_State return Machine_State;
   --  Allocate the required space for a Machine_State

   procedure Free_Machine_State (M : in out Machine_State);
   --  Free the dynamic memory taken by Machine_State

   --  The initial value of type Machine_State is created by the low level
   --  routine that actually raises an exception using the special builtin
   --  _builtin_machine_state. This value will typically encode the value
   --  of the program counter, and relevant registers. The following
   --  operations are defined on Machine_State values:

   function Get_Code_Loc (M : Machine_State) return Code_Loc;
   --  This function extracts the program counter value from a machine
   --  state, which the caller uses for searching the exception tables,
   --  and also for recording entries in the traceback table. The call
   --  returns a value of Null_Loc if the machine state represents the
   --  outer level, or some other frame for which no information can be
   --  provided.

   procedure Pop_Frame (M : Machine_State);
   --  This procedure pops the machine state M so that it represents the
   --  call point, as though the current subprogram had returned. It
   --  changes only the value referenced by M, and does not affect
   --  the current stack environment.

   function Fetch_Code (Loc : Code_Loc) return Code_Loc;
   --  Some architectures (notably VMS) use a descriptor to describe
   --  a subprogram address. This function computes the actual starting
   --  address of the code from Loc.
   --  Do not add pragma Inline, see 9116-002.
   --  ??? This function will go away when 'Code_Address is fixed on VMS.

   procedure Set_Machine_State (M : Machine_State);
   --  This routine sets M from the current machine state. It is called
   --  when an exception is initially signalled to initialize the state.

end System.Machine_State_Operations;
