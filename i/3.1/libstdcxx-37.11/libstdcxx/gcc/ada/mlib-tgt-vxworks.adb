------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                             M L I B . T G T                              --
--                            (VxWorks Version)                             --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--           Copyright (C) 2003-2005 Free Software Foundation, Inc.         --
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

--  This package provides a set of target dependent routines to build
--  static libraries.

--  This is the VxWorks version of the body

with MLib.Fil;
with Namet;  use Namet;
with Prj.Com;
with Sdefault;

package body MLib.Tgt is

   -----------------------
   -- Local Subprograms --
   -----------------------

   function Get_Target_Suffix return String;
   --  Returns the required suffix for some utilities
   --  (such as ar and ranlib) that depend on the real target.

   ---------------------
   -- Archive_Builder --
   ---------------------

   function Archive_Builder return String is
   begin
      return "ar" & Get_Target_Suffix;
   end Archive_Builder;

   -----------------------------
   -- Archive_Builder_Options --
   -----------------------------

   function Archive_Builder_Options return String_List_Access is
   begin
      return new String_List'(1 => new String'("cr"));
   end Archive_Builder_Options;

   -----------------
   -- Archive_Ext --
   -----------------

   function Archive_Ext return String is
   begin
      return "a";
   end Archive_Ext;

   ---------------------
   -- Archive_Indexer --
   ---------------------

   function Archive_Indexer return String is
   begin
      return "ranlib" & Get_Target_Suffix;
   end Archive_Indexer;

   -----------------------------
   -- Archive_Indexer_Options --
   -----------------------------

   function Archive_Indexer_Options return String_List_Access is
   begin
      return new String_List (1 .. 0);
   end Archive_Indexer_Options;

   ---------------------------
   -- Build_Dynamic_Library --
   ---------------------------

   procedure Build_Dynamic_Library
     (Ofiles       : Argument_List;
      Foreign      : Argument_List;
      Afiles       : Argument_List;
      Options      : Argument_List;
      Options_2    : Argument_List;
      Interfaces   : Argument_List;
      Lib_Filename : String;
      Lib_Dir      : String;
      Symbol_Data  : Symbol_Record;
      Driver_Name  : Name_Id := No_Name;
      Lib_Version  : String  := "";
      Auto_Init    : Boolean := False)
   is
      pragma Unreferenced (Ofiles);
      pragma Unreferenced (Foreign);
      pragma Unreferenced (Afiles);
      pragma Unreferenced (Options);
      pragma Unreferenced (Options_2);
      pragma Unreferenced (Interfaces);
      pragma Unreferenced (Lib_Filename);
      pragma Unreferenced (Lib_Dir);
      pragma Unreferenced (Symbol_Data);
      pragma Unreferenced (Driver_Name);
      pragma Unreferenced (Lib_Version);
      pragma Unreferenced (Auto_Init);

   begin
      null;
   end Build_Dynamic_Library;

   -------------
   -- DLL_Ext --
   -------------

   function DLL_Ext return String is
   begin
      return "";
   end DLL_Ext;

   ----------------
   -- DLL_Prefix --
   ----------------

   function DLL_Prefix return String is
   begin
      return "lib";
   end DLL_Prefix;

   --------------------
   -- Dynamic_Option --
   --------------------

   function Dynamic_Option return String is
   begin
      return "";
   end Dynamic_Option;

   -----------------------------
   -- Get_Target_Suffix --
   -----------------------------

   function Get_Target_Suffix return String is
      Target_Name : constant String_Ptr := Sdefault.Target_Name;
      Index       : Positive   := Target_Name'First;

   begin
      while Index < Target_Name'Last
        and then Target_Name (Index + 1) /= '-'
      loop
         Index := Index + 1;
      end loop;

      if Target_Name (Target_Name'First .. Index) = "m68k" then
         return "68k";
      elsif Target_Name (Target_Name'First .. Index) = "mips" then
         return "mips";
      elsif Target_Name (Target_Name'First .. Index) = "powerpc" then
         return "ppc";
      elsif Target_Name (Target_Name'First .. Index) = "sparc" then
         return "sparc";
      elsif Target_Name (Target_Name'First .. Index) = "sparc64" then
         return "sparc64";
      elsif Target_Name (Target_Name'First .. Index) = "xscale" then
         return "arm";
      else
         return "";
      end if;
   end Get_Target_Suffix;

   -------------------
   -- Is_Object_Ext --
   -------------------

   function Is_Object_Ext (Ext : String) return Boolean is
   begin
      return Ext = ".o";
   end Is_Object_Ext;

   --------------
   -- Is_C_Ext --
   --------------

   function Is_C_Ext (Ext : String) return Boolean is
   begin
      return Ext = ".c";
   end Is_C_Ext;

   --------------------
   -- Is_Archive_Ext --
   --------------------

   function Is_Archive_Ext (Ext : String) return Boolean is
   begin
      return Ext = ".a";
   end Is_Archive_Ext;

   -------------
   -- Libgnat --
   -------------

   function Libgnat return String is
   begin
      return "libgnat.a";
   end Libgnat;

   ------------------------
   -- Library_Exists_For --
   ------------------------

   function Library_Exists_For
     (Project : Project_Id; In_Tree : Project_Tree_Ref) return Boolean
   is
   begin
      if not In_Tree.Projects.Table (Project).Library then
         Prj.Com.Fail ("INTERNAL ERROR: Library_Exists_For called " &
                       "for non library project");
         return False;

      else
         declare
            Lib_Dir : constant String :=
              Get_Name_String
                (In_Tree.Projects.Table (Project).Library_Dir);
            Lib_Name : constant String :=
              Get_Name_String
                (In_Tree.Projects.Table (Project).Library_Name);

         begin
            if In_Tree.Projects.Table (Project).Library_Kind =
              Static
            then
               return Is_Regular_File
                 (Lib_Dir & Directory_Separator & "lib" &
                  Fil.Ext_To (Lib_Name, Archive_Ext));

            else
               return Is_Regular_File
                 (Lib_Dir & Directory_Separator & "lib" &
                  Fil.Ext_To (Lib_Name, DLL_Ext));
            end if;
         end;
      end if;
   end Library_Exists_For;

   ---------------------------
   -- Library_File_Name_For --
   ---------------------------

   function Library_File_Name_For
     (Project : Project_Id;
      In_Tree : Project_Tree_Ref) return Name_Id
   is
   begin
      if not In_Tree.Projects.Table (Project).Library then
         Prj.Com.Fail ("INTERNAL ERROR: Library_File_Name_For called " &
                       "for non library project");
         return No_Name;

      else
         declare
            Lib_Name : constant String :=
              Get_Name_String
                (In_Tree.Projects.Table (Project).Library_Name);

         begin
            Name_Len := 3;
            Name_Buffer (1 .. Name_Len) := "lib";

            if In_Tree.Projects.Table (Project).Library_Kind =
              Static
            then
               Add_Str_To_Name_Buffer (Fil.Ext_To (Lib_Name, Archive_Ext));

            else
               Add_Str_To_Name_Buffer (Fil.Ext_To (Lib_Name, DLL_Ext));
            end if;

            return Name_Find;
         end;
      end if;
   end Library_File_Name_For;

   ----------------
   -- Object_Ext --
   ----------------

   function Object_Ext return String is
   begin
      return "o";
   end Object_Ext;

   ----------------
   -- PIC_Option --
   ----------------

   function PIC_Option return String is
   begin
      return "";
   end PIC_Option;

   -----------------------------------------------
   -- Standalone_Library_Auto_Init_Is_Supported --
   -----------------------------------------------

   function Standalone_Library_Auto_Init_Is_Supported return Boolean is
   begin
      return False;
   end Standalone_Library_Auto_Init_Is_Supported;

   ---------------------------
   -- Support_For_Libraries --
   ---------------------------

   function Support_For_Libraries return Library_Support is
   begin
      return Static_Only;
   end Support_For_Libraries;

end MLib.Tgt;
