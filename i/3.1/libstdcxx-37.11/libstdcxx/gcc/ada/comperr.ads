------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                              C O M P E R R                               --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--          Copyright (C) 1992-2003 Free Software Foundation, Inc.          --
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

--  This package contains the routine called when a fatal internal compiler
--  error is detected. Calls to this routines cause termination of the
--  current compilation with appropriate error output.

package Comperr is

   procedure Compiler_Abort
     (X    : String;
      Code : Integer := 0);
   --  Signals an internal compiler error. Never returns control. Depending
   --  on processing may end up raising Unrecoverable_Error, or exiting
   --  directly. The message output is a "bug box" containing the
   --  string passed as an argument. The node in Current_Error_Node is used
   --  to provide the location where the error should be signalled. The
   --  message includes the node id, and the code parameter if it is positive.
   --  Note that this is only used at the outer level (to handle constraint
   --  errors or assert errors etc.) In the normal logic of the compiler we
   --  always use pragma Assert to check for errors, and if necessary an
   --  explicit abort is achieved by pragma Assert (False). Code is positive
   --  for a gigi abort (giving the gigi abort code), zero for a front
   --  end exception (with possible message stored in TSD.Current_Excep,
   --  and negative (an unused value) for a GCC abort.

   ------------------------------
   -- Use of gnat_bug.box File --
   ------------------------------

   --  When comperr generates the "bug box". The first two lines contain
   --  information on the version number, type of abort, and source location.

   --  Normally the remaining text is one of three possible forms
   --  depending on Gnatvsn.Gnat_Version_Type (FSF, Public, GNATPRO).
   --  See body of this package for the exact text used.

   --  In addition, an alternative mechanism exists for easily substituting
   --  different text for this message. Compiler_Abort checks for the
   --  existence of the file "gnat_bug.box" in the current source path.
   --  Most typically this file, if present, will be in the directory
   --  containing the run-time sources.

   --  If this file is present, then it is a plain ASCII file, whose
   --  contents replace the remaining text. The lines in this file should be
   --  72 characters or less to avoid misformatting the right boundary of the
   --  box. Note that the file does not contain the vertical bar characters or
   --  any leading spaces in lines.

end Comperr;
