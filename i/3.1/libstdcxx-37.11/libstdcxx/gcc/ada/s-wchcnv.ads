------------------------------------------------------------------------------
--                                                                          --
--                         GNAT RUN-TIME COMPONENTS                         --
--                                                                          --
--                       S Y S T E M . W C H _ C N V                        --
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

--  This unit may be used directly from an application program by providing
--  an appropriate WITH, and the interface can be expected to remain stable.

with System.WCh_Con;

package System.WCh_Cnv is
   pragma Pure;

   type UTF_32_Code is range 0 .. 16#7FFF_FFFF#;
   for UTF_32_Code'Size use 32;
   --  Range of allowed UTF-32 encoding values

   generic
      with function In_Char return Character;
   function Char_Sequence_To_Wide_Char
     (C  : Character;
      EM : System.WCh_Con.WC_Encoding_Method) return Wide_Character;
   --  C is the first character of a sequence of one or more characters which
   --  represent a wide character sequence. Calling the function In_Char for
   --  additional characters as required, Char_To_Wide_Char returns the
   --  corresponding wide character value. Constraint_Error is raised if the
   --  sequence of characters encountered is not a valid wide character
   --  sequence for the given encoding method.

   generic
      with function In_Char return Character;
   function Char_Sequence_To_UTF_32
     (C  : Character;
      EM : System.WCh_Con.WC_Encoding_Method) return UTF_32_Code;
   --  This is similar to the above, but the function returns a code from
   --  the full UTF_32 code set, which covers the full range of possible
   --  values in Wide_Wide_Character. The result can be converted to
   --  Wide_Wide_Character form using Wide_Wide_Character'Val.

   generic
      with procedure Out_Char (C : Character);
   procedure Wide_Char_To_Char_Sequence
     (WC : Wide_Character;
      EM : System.WCh_Con.WC_Encoding_Method);
   --  Given a wide character, converts it into a sequence of one or
   --  more characters, calling the given Out_Char procedure for each.
   --  Constraint_Error is raised if the given wide character value is
   --  not a valid value for the given encoding method.

   generic
      with procedure Out_Char (C : Character);
   procedure UTF_32_To_Char_Sequence
     (Val : UTF_32_Code;
      EM  : System.WCh_Con.WC_Encoding_Method);
   --  This is similar to the above, but the input value is a code from the
   --  full UTF_32 code set, which covers the full range of possible values
   --  in Wide_Wide_Character. To convert a Wide_Wide_Character value, the
   --  caller can use Wide_Wide_Character'Pos in the call.

end System.WCh_Cnv;
