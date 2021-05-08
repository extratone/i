------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                             S E M _ C H 1 2                              --
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

with Inline; use Inline;
with Types;  use Types;

package Sem_Ch12 is
   procedure Analyze_Generic_Package_Declaration        (N : Node_Id);
   procedure Analyze_Generic_Subprogram_Declaration     (N : Node_Id);
   procedure Analyze_Package_Instantiation              (N : Node_Id);
   procedure Analyze_Procedure_Instantiation            (N : Node_Id);
   procedure Analyze_Function_Instantiation             (N : Node_Id);
   procedure Analyze_Formal_Object_Declaration          (N : Node_Id);
   procedure Analyze_Formal_Type_Declaration            (N : Node_Id);
   procedure Analyze_Formal_Subprogram                  (N : Node_Id);
   procedure Analyze_Formal_Package                     (N : Node_Id);

   procedure Start_Generic;
   --  Must be invoked before starting to process a generic spec or body

   procedure End_Generic;
   --  Must be invoked just at the end of the end of the processing of a
   --  generic spec or body.

   procedure Check_Generic_Child_Unit
     (Gen_Id           : Node_Id;
      Parent_Installed : in out Boolean);
   --  If the name of the generic unit in an instantiation or a renaming
   --  is a selected component, then the prefix may be an instance and the
   --  selector may  designate a child unit. Retrieve the parent generic
   --  and search for the child unit that must be declared within. Similarly,
   --  if this is the name of a generic child unit within an instantiation of
   --  its own parent, retrieve the parent generic.

   function Copy_Generic_Node
     (N             : Node_Id;
      Parent_Id     : Node_Id;
      Instantiating : Boolean)
      return          Node_Id;
   --  Copy the tree for a generic unit or its body. The unit is copied
   --  repeatedly: once to produce a copy on which semantic analysis of
   --  the generic is performed, and once for each instantiation. The tree
   --  being copied is not semantically analyzed, except that references to
   --  global entities are marked on terminal nodes.

   function Get_Instance_Of (A : Entity_Id) return Entity_Id;
   --  Retrieve actual associated with given generic parameter.
   --  If A is uninstantiated or not a generic parameter, return A.

   function Get_Package_Instantiation_Node (A : Entity_Id) return Node_Id;
   --  Given the entity of a unit that is an instantiation, retrieve the
   --  original instance node. This is used when loading the instantiations
   --  of the ancestors of a child generic that is being instantiated.

   procedure Instantiate_Package_Body
     (Body_Info    : Pending_Body_Info;
      Inlined_Body : Boolean := False);
   --  Called after semantic analysis, to complete the instantiation of
   --  package instances. The flag Inlined_Body is set if the body is
   --  being instantiated on the fly for inlined purposes.

   procedure Instantiate_Subprogram_Body
     (Body_Info : Pending_Body_Info);
   --  Called after semantic analysis, to complete the instantiation of
   --  function and procedure instances.

   procedure Save_Global_References (N : Node_Id);
   --  Traverse the original generic unit, and capture all references to
   --  entities that are defined outside of the generic in the analyzed
   --  tree for the template. These references are copied into the original
   --  tree, so that they appear automatically in every instantiation.
   --  A critical invariant in this approach is that if an id in the generic
   --  resolves to a local entity, the corresponding id in the instance
   --  will resolve to the homologous entity in the instance, even though
   --  the enclosing context for resolution is different, as long as the
   --  global references have been captured as described here.

   --  Because instantiations can be nested, the environment of the instance,
   --  involving the actuals and other data-structures, must be saved and
   --  restored in stack-like fashion. Front-end inlining also uses these
   --  structures for the management of private/full views.

   procedure Set_Copied_Sloc_For_Inlined_Body (N : Node_Id; E : Entity_Id);
   --  This procedure is used when a subprogram body is inlined. This process
   --  shares the same circuitry as the creation of an instantiated copy of
   --  a generic template. The call to this procedure establishes a new source
   --  file entry representing the inlined body as an instantiation, marked as
   --  an inlined body (so that errout can distinguish cases for generating
   --  error messages, otherwise the treatment is identical). In this call
   --  N is the subprogram body and E is the defining identifier of the
   --  subprogram in quiestion. The resulting Sloc adjustment factor is
   --  saved as part of the internal state of the Sem_Ch12 package for use
   --  in subsequent calls to copy nodes.

   procedure Save_Env
     (Gen_Unit : Entity_Id;
      Act_Unit : Entity_Id);
   --   ??? comment needed

   procedure Restore_Env;
   --   ??? comment needed

   procedure Initialize;
   --  Initializes internal data structures

end Sem_Ch12;
