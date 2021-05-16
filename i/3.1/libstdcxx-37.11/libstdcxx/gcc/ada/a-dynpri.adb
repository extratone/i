------------------------------------------------------------------------------
--                                                                          --
--                  GNAT RUN-TIME LIBRARY (GNARL) COMPONENTS                --
--                                                                          --
--                 A D A . D Y N A M I C _ P R I O R I T I E S              --
--                                                                          --
--                                  B o d y                                 --
--                                                                          --
--          Copyright (C) 1992-2006, Free Software Foundation, Inc.         --
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

with System.Task_Primitives.Operations;
--  used for Write_Lock
--           Unlock
--           Set_Priority
--           Wakeup
--           Self

with System.Tasking;
--  used for Task_Id

with System.Parameters;
--  used for Single_Lock

with System.Soft_Links;
--  use for Abort_Defer
--          Abort_Undefer

with Unchecked_Conversion;

package body Ada.Dynamic_Priorities is

   package STPO renames System.Task_Primitives.Operations;
   package SSL renames System.Soft_Links;

   use System.Parameters;
   use System.Tasking;

   function Convert_Ids is new
     Unchecked_Conversion
       (Task_Identification.Task_Id, System.Tasking.Task_Id);

   ------------------
   -- Get_Priority --
   ------------------

   --  Inquire base priority of a task

   function Get_Priority
     (T : Ada.Task_Identification.Task_Id :=
        Ada.Task_Identification.Current_Task) return System.Any_Priority
   is
      Target : constant Task_Id := Convert_Ids (T);
      Error_Message : constant String := "Trying to get the priority of a ";

   begin
      if Target = Convert_Ids (Ada.Task_Identification.Null_Task_Id) then
         raise Program_Error with Error_Message & "null task";
      end if;

      if Task_Identification.Is_Terminated (T) then
         raise Tasking_Error with Error_Message & "null task";
      end if;

      return Target.Common.Base_Priority;
   end Get_Priority;

   ------------------
   -- Set_Priority --
   ------------------

   --  Change base priority of a task dynamically

   procedure Set_Priority
     (Priority : System.Any_Priority;
      T        : Ada.Task_Identification.Task_Id :=
                   Ada.Task_Identification.Current_Task)
   is
      Target  : constant Task_Id := Convert_Ids (T);
      Self_ID : constant Task_Id := STPO.Self;
      Error_Message : constant String := "Trying to set the priority of a ";

   begin
      if Target = Convert_Ids (Ada.Task_Identification.Null_Task_Id) then
         raise Program_Error with Error_Message & "null task";
      end if;

      if Task_Identification.Is_Terminated (T) then
         raise Tasking_Error with Error_Message & "terminated task";
      end if;

      SSL.Abort_Defer.all;

      if Single_Lock then
         STPO.Lock_RTS;
      end if;

      STPO.Write_Lock (Target);

      if Self_ID = Target then
         Target.Common.Base_Priority := Priority;
         STPO.Set_Priority (Target, Priority);

         STPO.Unlock (Target);

         if Single_Lock then
            STPO.Unlock_RTS;
         end if;

         --  Yield is needed to enforce FIFO task dispatching

         --  LL Set_Priority is made while holding the RTS lock so that it
         --  is inheriting high priority until it release all the RTS locks.

         --  If this is used in a system where Ceiling Locking is
         --  not enforced we may end up getting two Yield effects.

         STPO.Yield;

      else
         Target.New_Base_Priority := Priority;
         Target.Pending_Priority_Change := True;
         Target.Pending_Action := True;

         STPO.Wakeup (Target, Target.Common.State);

         --  If the task is suspended, wake it up to perform the change.
         --  check for ceiling violations ???

         STPO.Unlock (Target);

         if Single_Lock then
            STPO.Unlock_RTS;
         end if;
      end if;

      SSL.Abort_Undefer.all;
   end Set_Priority;

end Ada.Dynamic_Priorities;
