------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                              P R J . P R O C                             --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--          Copyright (C) 2001-2005, Free Software Foundation, Inc.         --
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

--  This package is used to convert a project file tree (see prj-tree.ads) to
--  project file data structures (see prj.ads), taking into account the
--  environment (external references).

with Prj.Tree;  use Prj.Tree;

package Prj.Proc is

   procedure Process
     (In_Tree                : Project_Tree_Ref;
      Project                : out Project_Id;
      Success                : out Boolean;
      From_Project_Node      : Project_Node_Id;
      From_Project_Node_Tree : Project_Node_Tree_Ref;
      Report_Error           : Put_Line_Access;
      Follow_Links           : Boolean := True;
      When_No_Sources        : Error_Warning := Error);
   --  Process a project file tree into project file data structures. If
   --  Report_Error is null, use the error reporting mechanism. Otherwise,
   --  report errors using Report_Error.
   --
   --  If Follow_Links is False, it is assumed that the project doesn't contain
   --  any file duplicated through symbolic links (although the latter are
   --  still valid if they point to a file which is outside of the project),
   --  and that no directory has a name which is a valid source name.
   --
   --  When_No_Sources indicates what should be done when no sources
   --  are found in a project for a specified or implied language.
   --
   --  Process is a bit of a junk name, how about Process_Project_Tree???

end Prj.Proc;
