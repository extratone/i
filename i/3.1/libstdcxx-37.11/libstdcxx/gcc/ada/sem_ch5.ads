------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                              S E M _ C H 5                               --
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

with Types; use Types;

package Sem_Ch5 is

   procedure Analyze_Assignment                 (N : Node_Id);
   procedure Analyze_Block_Statement            (N : Node_Id);
   procedure Analyze_Case_Statement             (N : Node_Id);
   procedure Analyze_Exit_Statement             (N : Node_Id);
   procedure Analyze_Goto_Statement             (N : Node_Id);
   procedure Analyze_If_Statement               (N : Node_Id);
   procedure Analyze_Implicit_Label_Declaration (N : Node_Id);
   procedure Analyze_Label                      (N : Node_Id);
   procedure Analyze_Loop_Statement             (N : Node_Id);
   procedure Analyze_Null_Statement             (N : Node_Id);
   procedure Analyze_Statements                 (L : List_Id);

   procedure Analyze_Label_Entity (E : Entity_Id);
   --  This procedure performs direct analysis of the label entity E. It
   --  is used when a label is created by the expander without bothering
   --  to insert an N_Implicit_Label_Declaration in the tree. It also takes
   --  care of setting Reachable, since labels defined by the expander can
   --  be assumed to be reachable.

   procedure Check_Possible_Current_Value_Condition (Cnode : Node_Id);
   --  Cnode is N_If_Statement, N_Elsif_Part, or N_Iteration_Scheme
   --  (the latter when a WHILE condition is present). This call checks
   --  if Condition (Cnode) is of the form ([NOT] var op val), where var
   --  is a simple object, val is known at compile time, and op is one
   --  of the six relational operators. If this is the case, and the
   --  Current_Value field of "var" is not set, then it is set to Cnode.
   --  See Exp_Util.Set_Current_Value_Condition for further details.

   procedure Check_Unreachable_Code (N : Node_Id);
   --  This procedure is called with N being the node for a statement that
   --  is an unconditional transfer of control. It checks to see if the
   --  statement is followed by some other statement, and if so generates
   --  an appropriate warning for unreachable code.

end Sem_Ch5;
