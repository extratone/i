------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                             E X P _ S M E M                              --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--          Copyright (C) 1998-2006, Free Software Foundation, Inc.         --
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

with Atree;    use Atree;
with Einfo;    use Einfo;
with Exp_Util; use Exp_Util;
with Nmake;    use Nmake;
with Namet;    use Namet;
with Nlists;   use Nlists;
with Rtsfind;  use Rtsfind;
with Sem;      use Sem;
with Sem_Util; use Sem_Util;
with Sinfo;    use Sinfo;
with Snames;   use Snames;
with Stand;    use Stand;
with Stringt;  use Stringt;
with Tbuild;   use Tbuild;

package body Exp_Smem is

   Insert_Node : Node_Id;
   --  Node after which a write call is to be inserted

   -----------------------
   -- Local Subprograms --
   -----------------------

   procedure Add_Read_Before (N : Node_Id);
   --  Insert a Shared_Var_ROpen call for variable before node N

   procedure Add_Write_After (N : Node_Id);
   --  Insert a Shared_Var_WOpen call for variable after the node
   --  Insert_Node, as recorded by On_Lhs_Of_Assigment (where it points
   --  to the assignment statement) or Is_Out_Actual (where it points to
   --  the procedure call statement).

   procedure Build_Full_Name (E : Entity_Id; N : out String_Id);
   --  Build the fully qualified string name of a shared variable

   function On_Lhs_Of_Assignment (N : Node_Id) return Boolean;
   --  Determines if N is on the left hand of the assignment. This means
   --  that either it is a simple variable, or it is a record or array
   --  variable with a corresponding selected or indexed component on
   --  the left side of an assignment. If the result is True, then
   --  Insert_Node is set to point to the assignment

   function Is_Out_Actual (N : Node_Id) return Boolean;
   --  In a similar manner, this function determines if N appears as an
   --  OUT or IN OUT parameter to a procedure call. If the result is
   --  True, then Insert_Node is set to point to the assignment.

   ---------------------
   -- Add_Read_Before --
   ---------------------

   procedure Add_Read_Before (N : Node_Id) is
      Loc : constant Source_Ptr := Sloc (N);
      Ent : constant Node_Id    := Entity (N);

   begin
      if Present (Shared_Var_Read_Proc (Ent)) then
         Insert_Action (N,
           Make_Procedure_Call_Statement (Loc,
             Name =>
               New_Occurrence_Of (Shared_Var_Read_Proc (Ent), Loc),
             Parameter_Associations => Empty_List));
      end if;
   end Add_Read_Before;

   -------------------------------
   -- Add_Shared_Var_Lock_Procs --
   -------------------------------

   procedure Add_Shared_Var_Lock_Procs (N : Node_Id) is
      Loc   : constant Source_Ptr := Sloc (N);
      Obj   : constant Entity_Id  := Entity (Expression (First_Actual (N)));
      Inode : Node_Id;
      Vnm   : String_Id;

   begin
      --  We have to add Shared_Var_Lock and Shared_Var_Unlock calls around
      --  the procedure or function call node. First we locate the right
      --  place to do the insertion, which is the call itself in the
      --  procedure call case, or else the nearest non subexpression
      --  node that contains the function call.

      Inode := N;
      while Nkind (Inode) /= N_Procedure_Call_Statement
        and then Nkind (Inode) in N_Subexpr
      loop
         Inode := Parent (Inode);
      end loop;

      --  Now insert the Lock and Unlock calls and the read/write calls

      --  Two concerns here. First we are not dealing with the exception
      --  case, really we need some kind of cleanup routine to do the
      --  Unlock. Second, these lock calls should be inside the protected
      --  object processing, not outside, otherwise they can be done at
      --  the wrong priority, resulting in dead lock situations ???

      Build_Full_Name (Obj, Vnm);

      --  First insert the Lock call before

      Insert_Before_And_Analyze (Inode,
        Make_Procedure_Call_Statement (Loc,
          Name => New_Occurrence_Of (RTE (RE_Shared_Var_Lock), Loc),
          Parameter_Associations => New_List (
            Make_String_Literal (Loc, Vnm))));

      --  Now, right after the Lock, insert a call to read the object

      Insert_Before_And_Analyze (Inode,
        Make_Procedure_Call_Statement (Loc,
          Name => New_Occurrence_Of (Shared_Var_Read_Proc (Obj), Loc)));

      --  Now insert the Unlock call after

      Insert_After_And_Analyze (Inode,
        Make_Procedure_Call_Statement (Loc,
          Name => New_Occurrence_Of (RTE (RE_Shared_Var_Unlock), Loc),
          Parameter_Associations => New_List (
            Make_String_Literal (Loc, Vnm))));

      --  Now for a procedure call, but not a function call, insert the
      --  call to write the object just before the unlock.

      if Nkind (N) = N_Procedure_Call_Statement then
         Insert_After_And_Analyze (Inode,
           Make_Procedure_Call_Statement (Loc,
             Name => New_Occurrence_Of (Shared_Var_Assign_Proc (Obj), Loc)));
      end if;

   end Add_Shared_Var_Lock_Procs;

   ---------------------
   -- Add_Write_After --
   ---------------------

   procedure Add_Write_After (N : Node_Id) is
      Loc : constant Source_Ptr := Sloc (N);
      Ent : constant Node_Id    := Entity (N);

   begin
      if Present (Shared_Var_Assign_Proc (Ent)) then
         Insert_After_And_Analyze (Insert_Node,
           Make_Procedure_Call_Statement (Loc,
             Name =>
               New_Occurrence_Of (Shared_Var_Assign_Proc (Ent), Loc),
             Parameter_Associations => Empty_List));
      end if;
   end Add_Write_After;

   ---------------------
   -- Build_Full_Name --
   ---------------------

   procedure Build_Full_Name (E : Entity_Id; N : out String_Id) is

      procedure Build_Name (E : Entity_Id);
      --  This is a recursive routine used to construct the fully qualified
      --  string name of the package corresponding to the shared variable.

      ----------------
      -- Build_Name --
      ----------------

      procedure Build_Name (E : Entity_Id) is
      begin
         if Scope (E) /= Standard_Standard then
            Build_Name (Scope (E));
            Store_String_Char ('.');
         end if;

         Get_Decoded_Name_String (Chars (E));
         Store_String_Chars (Name_Buffer (1 .. Name_Len));
      end Build_Name;

   --  Start of processing for Build_Full_Name

   begin
      Start_String;
      Build_Name (E);
      N := End_String;
   end Build_Full_Name;

   ------------------------------------
   -- Expand_Shared_Passive_Variable --
   ------------------------------------

   procedure Expand_Shared_Passive_Variable (N : Node_Id) is
      Typ : constant Entity_Id := Etype (N);

   begin
      --  Nothing to do for protected or limited objects

      if Is_Limited_Type (Typ) or else Is_Concurrent_Type (Typ) then
         return;

      --  If we are on the left hand side of an assignment, then we add
      --  the write call after the assignment.

      elsif On_Lhs_Of_Assignment (N) then
         Add_Write_After (N);

      --  If we are a parameter for an out or in out formal, then put
      --  the read before and the write after.

      elsif Is_Out_Actual (N) then
         Add_Read_Before (N);
         Add_Write_After (N);

      --  All other cases are simple reads

      else
         Add_Read_Before (N);
      end if;
   end Expand_Shared_Passive_Variable;

   -------------------
   -- Is_Out_Actual --
   -------------------

   function Is_Out_Actual (N : Node_Id) return Boolean is
      Parnt  : constant Node_Id := Parent (N);
      Formal : Entity_Id;
      Call   : Node_Id;
      Actual : Node_Id;

   begin
      if (Nkind (Parnt) = N_Indexed_Component
            or else
          Nkind (Parnt) = N_Selected_Component)
        and then N = Prefix (Parnt)
      then
         return Is_Out_Actual (Parnt);

      elsif Nkind (Parnt) = N_Parameter_Association
        and then N = Explicit_Actual_Parameter (Parnt)
      then
         Call := Parent (Parnt);

      elsif Nkind (Parnt) = N_Procedure_Call_Statement then
         Call := Parnt;

      else
         return False;
      end if;

      --  Fall here if we are definitely a parameter

      Actual := First_Actual (Call);
      Formal := First_Formal (Entity (Name (Call)));

      loop
         if Actual = N then
            if Ekind (Formal) /= E_In_Parameter then
               Insert_Node := Call;
               return True;
            else
               return False;
            end if;

         else
            Actual := Next_Actual (Actual);
            Formal := Next_Formal (Formal);
         end if;
      end loop;
   end Is_Out_Actual;

   ---------------------------
   -- Make_Shared_Var_Procs --
   ---------------------------

   procedure Make_Shared_Var_Procs (N : Node_Id) is
      Loc : constant Source_Ptr := Sloc (N);
      Ent : constant Entity_Id  := Defining_Identifier (N);
      Typ : constant Entity_Id  := Etype (Ent);
      Vnm : String_Id;
      Atr : Node_Id;

      Assign_Proc : constant Entity_Id :=
                      Make_Defining_Identifier (Loc,
                        Chars => New_External_Name (Chars (Ent), 'A'));

      Read_Proc : constant Entity_Id :=
                    Make_Defining_Identifier (Loc,
                      Chars => New_External_Name (Chars (Ent), 'R'));

      S : Entity_Id;

   --  Start of processing for Make_Shared_Var_Procs

   begin
      Build_Full_Name (Ent, Vnm);

      --  We turn off Shared_Passive during construction and analysis of
      --  the assign and read routines, to avoid improper attempts to
      --  process the variable references within these procedures.

      Set_Is_Shared_Passive (Ent, False);

      --  Construct assignment routine

      --    procedure VarA is
      --       S : Ada.Streams.Stream_IO.Stream_Access;
      --    begin
      --       S := Shared_Var_WOpen ("pkg.var");
      --       typ'Write (S, var);
      --       Shared_Var_Close (S);
      --    end VarA;

      S   := Make_Defining_Identifier (Loc, Name_uS);

      Atr :=
        Make_Attribute_Reference (Loc,
          Prefix => New_Occurrence_Of (Typ, Loc),
          Attribute_Name => Name_Write,
          Expressions => New_List (
            New_Reference_To (S, Loc),
            New_Occurrence_Of (Ent, Loc)));

      Insert_After_And_Analyze (N,
        Make_Subprogram_Body (Loc,
          Specification =>
            Make_Procedure_Specification (Loc,
              Defining_Unit_Name => Assign_Proc),

         --  S : Ada.Streams.Stream_IO.Stream_Access;

          Declarations => New_List (
            Make_Object_Declaration (Loc,
              Defining_Identifier => S,
              Object_Definition =>
                New_Occurrence_Of (RTE (RE_Stream_Access), Loc))),

          Handled_Statement_Sequence =>
            Make_Handled_Sequence_Of_Statements (Loc,
              Statements => New_List (

               --  S := Shared_Var_WOpen ("pkg.var");

                Make_Assignment_Statement (Loc,
                  Name => New_Reference_To (S, Loc),
                  Expression =>
                    Make_Function_Call (Loc,
                      Name =>
                        New_Occurrence_Of
                          (RTE (RE_Shared_Var_WOpen), Loc),
                      Parameter_Associations => New_List (
                        Make_String_Literal (Loc, Vnm)))),

                Atr,

               --  Shared_Var_Close (S);

                Make_Procedure_Call_Statement (Loc,
                  Name =>
                    New_Occurrence_Of (RTE (RE_Shared_Var_Close), Loc),
                  Parameter_Associations =>
                    New_List (New_Reference_To (S, Loc)))))));

      --  Construct read routine

      --    procedure varR is
      --       S : Ada.Streams.Stream_IO.Stream_Access;
      --    begin
      --       S := Shared_Var_ROpen ("pkg.var");
      --       if S /= null then
      --          typ'Read (S, Var);
      --          Shared_Var_Close (S);
      --       end if;
      --    end varR;

      S   := Make_Defining_Identifier (Loc, Name_uS);

      Atr :=
        Make_Attribute_Reference (Loc,
          Prefix => New_Occurrence_Of (Typ, Loc),
          Attribute_Name => Name_Read,
          Expressions => New_List (
            New_Reference_To (S, Loc),
            New_Occurrence_Of (Ent, Loc)));

      Insert_After_And_Analyze (N,
        Make_Subprogram_Body (Loc,
          Specification =>
            Make_Procedure_Specification (Loc,
              Defining_Unit_Name => Read_Proc),

         --  S : Ada.Streams.Stream_IO.Stream_Access;

          Declarations => New_List (
            Make_Object_Declaration (Loc,
              Defining_Identifier => S,
              Object_Definition =>
                New_Occurrence_Of (RTE (RE_Stream_Access), Loc))),

          Handled_Statement_Sequence =>
            Make_Handled_Sequence_Of_Statements (Loc,
              Statements => New_List (

               --  S := Shared_Var_ROpen ("pkg.var");

                Make_Assignment_Statement (Loc,
                  Name => New_Reference_To (S, Loc),
                  Expression =>
                    Make_Function_Call (Loc,
                      Name =>
                        New_Occurrence_Of
                          (RTE (RE_Shared_Var_ROpen), Loc),
                      Parameter_Associations => New_List (
                        Make_String_Literal (Loc, Vnm)))),

               --  if S /= null then

                Make_Implicit_If_Statement (N,
                  Condition =>
                    Make_Op_Ne (Loc,
                      Left_Opnd  => New_Reference_To (S, Loc),
                      Right_Opnd => Make_Null (Loc)),

                   Then_Statements => New_List (

                     --  typ'Read (S, Var);

                     Atr,

                     --  Shared_Var_Close (S);

                     Make_Procedure_Call_Statement (Loc,
                       Name =>
                         New_Occurrence_Of
                           (RTE (RE_Shared_Var_Close), Loc),
                       Parameter_Associations =>
                         New_List (New_Reference_To (S, Loc)))))))));

      Set_Is_Shared_Passive      (Ent, True);
      Set_Shared_Var_Assign_Proc (Ent, Assign_Proc);
      Set_Shared_Var_Read_Proc   (Ent, Read_Proc);
   end Make_Shared_Var_Procs;

   --------------------------
   -- On_Lhs_Of_Assignment --
   --------------------------

   function On_Lhs_Of_Assignment (N : Node_Id) return Boolean is
      P : constant Node_Id := Parent (N);

   begin
      if Nkind (P) = N_Assignment_Statement then
         if N = Name (P) then
            Insert_Node := P;
            return True;
         else
            return False;
         end if;

      elsif (Nkind (P) = N_Indexed_Component
               or else
             Nkind (P) = N_Selected_Component)
        and then N = Prefix (P)
      then
         return On_Lhs_Of_Assignment (P);

      else
         return False;
      end if;
   end On_Lhs_Of_Assignment;

end Exp_Smem;
