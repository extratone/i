------------------------------------------------------------------------------
--                                                                          --
--                 GNAT RUN-TIME LIBRARY (GNARL) COMPONENTS                 --
--                                                                          --
--    S Y S T E M . T A S K _ P R I M I T I V E S . O P E R A T I O N S .   --
--                                   D E C                                  --
--                                                                          --
--                                  B o d y                                 --
--                                                                          --
--           Copyright (C) 2000-2005 Free Software Foundation, Inc.         --
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

--   This package is for OpenVMS/Alpha

with System.OS_Interface;
with System.Parameters;
with System.Tasking;
with Unchecked_Conversion;
with System.Soft_Links;

package body System.Task_Primitives.Operations.DEC is

   use System.OS_Interface;
   use System.Parameters;
   use System.Tasking;
   use System.Aux_DEC;
   use type Interfaces.C.int;

   package SSL renames System.Soft_Links;

   --  The FAB_RAB_Type specifies where the context field (the calling
   --  task) is stored.  Other fields defined for FAB_RAB arent' need and
   --  so are ignored.

   type FAB_RAB_Type is record
      CTX : Unsigned_Longword;
   end record;

   for FAB_RAB_Type use record
      CTX at 24 range 0 .. 31;
   end record;

   for FAB_RAB_Type'Size use 224;

   type FAB_RAB_Access_Type is access all FAB_RAB_Type;

   -----------------------
   -- Local Subprograms --
   -----------------------

   pragma Warnings (Off);
   --  Task_Id is 64 bits wide (but only 32 bits significant) on Integrity/VMS

   function To_Unsigned_Longword is new
     Unchecked_Conversion (Task_Id, Unsigned_Longword);

   function To_Task_Id is new
     Unchecked_Conversion (Unsigned_Longword, Task_Id);

   pragma Warnings (On);

   function To_FAB_RAB is new
     Unchecked_Conversion (Address, FAB_RAB_Access_Type);

   ---------------------------
   -- Interrupt_AST_Handler --
   ---------------------------

   procedure Interrupt_AST_Handler (ID : Address) is
      Result      : Interfaces.C.int;
      AST_Self_ID : constant Task_Id := To_Task_Id (ID);
   begin
      Result := pthread_cond_signal_int_np (AST_Self_ID.Common.LL.CV'Access);
      pragma Assert (Result = 0);
   end Interrupt_AST_Handler;

   ---------------------
   -- RMS_AST_Handler --
   ---------------------

   procedure RMS_AST_Handler (ID : Address) is
      AST_Self_ID : constant Task_Id := To_Task_Id (To_FAB_RAB (ID).CTX);
      Result      : Interfaces.C.int;

   begin
      AST_Self_ID.Common.LL.AST_Pending := False;
      Result := pthread_cond_signal_int_np (AST_Self_ID.Common.LL.CV'Access);
      pragma Assert (Result = 0);
   end RMS_AST_Handler;

   ----------
   -- Self --
   ----------

   function Self return Unsigned_Longword is
      Self_ID : constant Task_Id := Self;
   begin
      Self_ID.Common.LL.AST_Pending := True;
      return To_Unsigned_Longword (Self);
   end Self;

   -------------------------
   -- Starlet_AST_Handler --
   -------------------------

   procedure Starlet_AST_Handler (ID : Address) is
      Result      : Interfaces.C.int;
      AST_Self_ID : constant Task_Id := To_Task_Id (ID);
   begin
      AST_Self_ID.Common.LL.AST_Pending := False;
      Result := pthread_cond_signal_int_np (AST_Self_ID.Common.LL.CV'Access);
      pragma Assert (Result = 0);
   end Starlet_AST_Handler;

   ----------------
   -- Task_Synch --
   ----------------

   procedure Task_Synch is
      Synch_Self_ID : constant Task_Id := Self;

   begin
      if Single_Lock then
         Lock_RTS;
      else
         Write_Lock (Synch_Self_ID);
      end if;

      SSL.Abort_Defer.all;
      Synch_Self_ID.Common.State := AST_Server_Sleep;

      while Synch_Self_ID.Common.LL.AST_Pending loop
         Sleep (Synch_Self_ID, AST_Server_Sleep);
      end loop;

      Synch_Self_ID.Common.State := Runnable;

      if Single_Lock then
         Unlock_RTS;
      else
         Unlock (Synch_Self_ID);
      end if;

      SSL.Abort_Undefer.all;
   end Task_Synch;

end System.Task_Primitives.Operations.DEC;
