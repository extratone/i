------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                             E X P _ C H 1 1                              --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--          Copyright (C) 1992-2005, Free Software Foundation, Inc.         --
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
-- GNAT was originally developed  by the GNAT team at  New York University. --
-- Extensive contributions were provided by Ada Core Technologies Inc.      --
--                                                                          --
------------------------------------------------------------------------------

--  Expand routines for chapter 11 constructs

with Types; use Types;

package Exp_Ch11 is
   procedure Expand_N_Exception_Declaration          (N : Node_Id);
   procedure Expand_N_Handled_Sequence_Of_Statements (N : Node_Id);
   procedure Expand_N_Raise_Constraint_Error         (N : Node_Id);
   procedure Expand_N_Raise_Program_Error            (N : Node_Id);
   procedure Expand_N_Raise_Statement                (N : Node_Id);
   procedure Expand_N_Raise_Storage_Error            (N : Node_Id);
   procedure Expand_N_Subprogram_Info                (N : Node_Id);

   --  Data structures for gathering information to build exception tables
   --  See runtime routine Ada.Exceptions for full details on the format and
   --  content of these tables.

   procedure Expand_At_End_Handler (HSS : Node_Id; Block : Node_Id);
   --  Given a handled statement sequence, HSS, for which the At_End_Proc
   --  field is set, and which currently has no exception handlers, this
   --  procedure expands the special exception handler required.
   --  This procedure also create a new scope for the given Block, if
   --  Block is not Empty.

   procedure Expand_Exception_Handlers (HSS : Node_Id);
   --  This procedure expands exception handlers, and is called as part
   --  of the processing for Expand_N_Handled_Sequence_Of_Statements and
   --  is also called from Expand_At_End_Handler. N is the handled sequence
   --  of statements that has the exception handler(s) to be expanded. This
   --  is also called to expand the special exception handler built for
   --  accept bodies (see Exp_Ch9.Build_Accept_Body).

   function Is_Non_Ada_Error (E : Entity_Id) return Boolean;
   --  This function is provided for Gigi use. It returns True if operating on
   --  VMS, and the argument E is the entity for System.Aux_Dec.Non_Ada_Error.
   --  This is used to generate the special matching code for this exception.

end Exp_Ch11;
