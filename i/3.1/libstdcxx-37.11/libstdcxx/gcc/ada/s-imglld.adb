------------------------------------------------------------------------------
--                                                                          --
--                         GNAT RUN-TIME COMPONENTS                         --
--                                                                          --
--                       S Y S T E M . I M G _ L L D                        --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--          Copyright (C) 1992-2005 Free Software Foundation, Inc.          --
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

with System.Img_Dec; use System.Img_Dec;
with System.Img_LLI; use System.Img_LLI;

package body System.Img_LLD is

   -----------------------------
   -- Image_Long_Long_Decimal --
   -----------------------------

   function Image_Long_Long_Decimal
     (V     : Long_Long_Integer;
      Scale : Integer)
      return  String
   is
      P : Natural := 0;
      S : String (1 .. 64);

   begin
      Set_Image_Long_Long_Decimal
        (V, S, P, Scale, 1, Integer'Max (1, Scale), 0);

      --  Mess around to make sure we have the objectionable space at the
      --  start for positive numbers in accordance with the annoying rules!

      if S (1) /= ' ' and then S (1) /= '-' then
         S (2 .. P + 1) := S (1 .. P);
         S (1) := ' ';
         return S (1 .. P + 1);
      else
         return S (1 .. P);
      end if;
   end Image_Long_Long_Decimal;

   ---------------------------------
   -- Set_Image_Long_Long_Decimal --
   ---------------------------------

   procedure Set_Image_Long_Long_Decimal
     (V     : Long_Long_Integer;
      S     : out String;
      P     : in out Natural;
      Scale : Integer;
      Fore  : Natural;
      Aft   : Natural;
      Exp   : Natural)
   is
      Digs : String := Image_Long_Long_Integer (V);
      --  Sign and digits of decimal value

   begin
      Set_Decimal_Digits (Digs, Digs'Length, S, P, Scale, Fore, Aft, Exp);
   end Set_Image_Long_Long_Decimal;

end System.Img_LLD;
