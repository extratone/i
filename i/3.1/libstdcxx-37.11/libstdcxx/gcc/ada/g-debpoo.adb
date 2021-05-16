------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                       G N A T . D E B U G _ P O O L S                    --
--                                                                          --
--                                 B o d y                                  --
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

with Ada.Exceptions.Traceback;
with GNAT.IO; use GNAT.IO;

with System.Address_Image;
with System.Memory;     use System.Memory;
with System.Soft_Links; use System.Soft_Links;

with System.Traceback_Entries; use System.Traceback_Entries;

with GNAT.HTable;
with GNAT.Traceback; use GNAT.Traceback;

with Ada.Unchecked_Conversion;

package body GNAT.Debug_Pools is

   Default_Alignment : constant := Standard'Maximum_Alignment;
   --  Alignment used for the memory chunks returned by Allocate. Using this
   --  value garantees that this alignment will be compatible with all types
   --  and at the same time makes it easy to find the location of the extra
   --  header allocated for each chunk.

   Initial_Memory_Size : constant Storage_Offset := 2 ** 26; --  64 Mb
   --  Initial size of memory that the debug pool can handle. This is used to
   --  compute the size of the htable used to monitor the blocks, but this is
   --  dynamic and will grow as needed. Having a bigger size here means a
   --  longer setup time, but less time spent later on to grow the array.

   Max_Ignored_Levels : constant Natural := 10;
   --  Maximum number of levels that will be ignored in backtraces. This is so
   --  that we still have enough significant levels in the tracebacks returned
   --  to the user.
   --
   --  The value 10 is chosen as being greater than the maximum callgraph
   --  in this package. Its actual value is not really relevant, as long as it
   --  is high enough to make sure we still have enough frames to return to
   --  the user after we have hidden the frames internal to this package.

   ---------------------------
   -- Back Trace Hash Table --
   ---------------------------

   --  This package needs to store one set of tracebacks for each allocation
   --  point (when was it allocated or deallocated). This would use too much
   --  memory,  so the tracebacks are actually stored in a hash table, and
   --  we reference elements in this hash table instead.

   --  This hash-table will remain empty if the discriminant Stack_Trace_Depth
   --  for the pools is set to 0.

   --  This table is a global table, that can be shared among all debug pools
   --  with no problems.

   type Header is range 1 .. 1023;
   --  Number of elements in the hash-table

   type Tracebacks_Array_Access
      is access GNAT.Traceback.Tracebacks_Array;

   type Traceback_Kind is (Alloc, Dealloc, Indirect_Alloc, Indirect_Dealloc);

   type Traceback_Htable_Elem;
   type Traceback_Htable_Elem_Ptr
      is access Traceback_Htable_Elem;

   type Traceback_Htable_Elem is record
      Traceback : Tracebacks_Array_Access;
      Kind      : Traceback_Kind;
      Count     : Natural;
      Total     : Byte_Count;
      Next      : Traceback_Htable_Elem_Ptr;
   end record;

   --  Subprograms used for the Backtrace_Htable instantiation

   procedure Set_Next
     (E    : Traceback_Htable_Elem_Ptr;
      Next : Traceback_Htable_Elem_Ptr);
   pragma Inline (Set_Next);

   function Next
     (E : Traceback_Htable_Elem_Ptr) return Traceback_Htable_Elem_Ptr;
   pragma Inline (Next);

   function Get_Key
     (E : Traceback_Htable_Elem_Ptr) return Tracebacks_Array_Access;
   pragma Inline (Get_Key);

   function Hash (T : Tracebacks_Array_Access) return Header;
   pragma Inline (Hash);

   function Equal (K1, K2 : Tracebacks_Array_Access) return Boolean;
   --  Why is this not inlined???

   --  The hash table for back traces

   package Backtrace_Htable is new GNAT.HTable.Static_HTable
     (Header_Num => Header,
      Element    => Traceback_Htable_Elem,
      Elmt_Ptr   => Traceback_Htable_Elem_Ptr,
      Null_Ptr   => null,
      Set_Next   => Set_Next,
      Next       => Next,
      Key        => Tracebacks_Array_Access,
      Get_Key    => Get_Key,
      Hash       => Hash,
      Equal      => Equal);

   -----------------------
   -- Allocations table --
   -----------------------

   type Allocation_Header;
   type Allocation_Header_Access is access Allocation_Header;

   type Traceback_Ptr_Or_Address is new System.Address;
   --  A type that acts as a C union, and is either a System.Address or a
   --  Traceback_Htable_Elem_Ptr.

   --  The following record stores extra information that needs to be
   --  memorized for each block allocated with the special debug pool.

   type Allocation_Header is record
      Allocation_Address : System.Address;
      --  Address of the block returned by malloc, possibly unaligned

      Block_Size : Storage_Offset;
      --  Needed only for advanced freeing algorithms (traverse all allocated
      --  blocks for potential references). This value is negated when the
      --  chunk of memory has been logically freed by the application. This
      --  chunk has not been physically released yet.

      Alloc_Traceback : Traceback_Htable_Elem_Ptr;
      --  ??? comment required

      Dealloc_Traceback : Traceback_Ptr_Or_Address;
      --  Pointer to the traceback for the allocation (if the memory chunk is
      --  still valid), or to the first deallocation otherwise. Make sure this
      --  is a thin pointer to save space.
      --
      --  Dealloc_Traceback is also for blocks that are still allocated to
      --  point to the previous block in the list. This saves space in this
      --  header, and make manipulation of the lists of allocated pointers
      --  faster.

      Next : System.Address;
      --  Point to the next block of the same type (either allocated or
      --  logically freed) in memory. This points to the beginning of the user
      --  data, and does not include the header of that block.
   end record;

   function Header_Of (Address : System.Address)
      return Allocation_Header_Access;
   pragma Inline (Header_Of);
   --  Return the header corresponding to a previously allocated address

   function To_Address is new Ada.Unchecked_Conversion
     (Traceback_Ptr_Or_Address, System.Address);

   function To_Address is new Ada.Unchecked_Conversion
     (System.Address, Traceback_Ptr_Or_Address);

   function To_Traceback is new Ada.Unchecked_Conversion
     (Traceback_Ptr_Or_Address, Traceback_Htable_Elem_Ptr);

   function To_Traceback is new Ada.Unchecked_Conversion
     (Traceback_Htable_Elem_Ptr, Traceback_Ptr_Or_Address);

   Header_Offset : constant Storage_Count :=
                     Default_Alignment *
                       ((Allocation_Header'Size / System.Storage_Unit
                          + Default_Alignment - 1) / Default_Alignment);
   --  Offset of user data after allocation header

   Minimum_Allocation : constant Storage_Count :=
                          Default_Alignment - 1 + Header_Offset;
   --  Minimal allocation: size of allocation_header rounded up to next
   --  multiple of default alignment + worst-case padding.

   -----------------------
   -- Allocations table --
   -----------------------

   --  This table is indexed on addresses modulo Default_Alignment, and for
   --  each index it indicates whether that memory block is valid. Its behavior
   --  is similar to GNAT.Table, except that we need to pack the table to save
   --  space, so we cannot reuse GNAT.Table as is.

   --  This table is the reason why all alignments have to be forced to common
   --  value (Default_Alignment), so that this table can be kept to a
   --  reasonnable size.

   type Byte is mod 2 ** System.Storage_Unit;

   Big_Table_Size : constant Storage_Offset :=
                      (Storage_Offset'Last - 1) / Default_Alignment;
   type Big_Table is array (0 .. Big_Table_Size) of Byte;
   --  A simple, flat-array type used to access memory bytes (see the comment
   --  for Valid_Blocks below).
   --
   --  It would be cleaner to represent this as a packed array of Boolean.
   --  However, we cannot specify pragma Pack for such an array, since the
   --  total size on a 64 bit machine would be too big (> Integer'Last).
   --
   --  Given an address, we know if it is under control of the debug pool if
   --  the byte at index:
   --       ((Address - Edata'Address) / Default_Alignment)
   --        / Storage_unit
   --  has the bit
   --       ((Address - Edata'Address) / Default_Alignment)
   --        mod Storage_Unit
   --  set to 1.
   --
   --  See the subprograms Is_Valid and Set_Valid for proper manipulation of
   --  this array.

   type Table_Ptr is access Big_Table;
   function To_Pointer is new Ada.Unchecked_Conversion
     (System.Address, Table_Ptr);

   Valid_Blocks      : Table_Ptr      := null;
   Valid_Blocks_Size : Storage_Offset := 0;
   --  These two variables represents a mapping of the currently allocated
   --  memory. Every time the pool works on an address, we first check that the
   --  index Address / Default_Alignment is True. If not, this means that this
   --  address is not under control of the debug pool and thus this is probably
   --  an invalid memory access (it could also be a general access type).
   --
   --  Note that in fact we never allocate the full size of Big_Table, only a
   --  slice big enough to manage the currently allocated memory.

   Edata : System.Address := System.Null_Address;
   --  Address in memory that matches the index 0 in Valid_Blocks. It is named
   --  after the symbol _edata, which, on most systems, indicate the lowest
   --  possible address returned by malloc. Unfortunately, this symbol doesn't
   --  exist on windows, so we cannot use it instead of this variable.

   -----------------------
   -- Local subprograms --
   -----------------------

   function Find_Or_Create_Traceback
     (Pool                : Debug_Pool;
      Kind                : Traceback_Kind;
      Size                : Storage_Count;
      Ignored_Frame_Start : System.Address;
      Ignored_Frame_End   : System.Address) return Traceback_Htable_Elem_Ptr;
   --  Return an element matching the current traceback (omitting the frames
   --  that are in the current package). If this traceback already existed in
   --  the htable, a pointer to this is returned to spare memory. Null is
   --  returned if the pool is set not to store tracebacks. If the traceback
   --  already existed in the table, the count is incremented so that
   --  Dump_Tracebacks returns useful results. All addresses up to, and
   --  including, an address between Ignored_Frame_Start .. Ignored_Frame_End
   --  are ignored.

   procedure Put_Line
     (Depth               : Natural;
      Traceback           : Tracebacks_Array_Access;
      Ignored_Frame_Start : System.Address := System.Null_Address;
      Ignored_Frame_End   : System.Address := System.Null_Address);
   --  Print Traceback to Standard_Output. If Traceback is null, print the
   --  call_chain at the current location, up to Depth levels, ignoring all
   --  addresses up to the first one in the range
   --  Ignored_Frame_Start .. Ignored_Frame_End

   function Is_Valid (Storage : System.Address) return Boolean;
   pragma Inline (Is_Valid);
   --  Return True if Storage is an address that the debug pool has under its
   --  control.

   procedure Set_Valid (Storage : System.Address; Value : Boolean);
   pragma Inline (Set_Valid);
   --  Mark the address Storage as being under control of the memory pool (if
   --  Value is True), or not (if Value is False). This procedure will
   --  reallocate the table Valid_Blocks as needed.

   procedure Set_Dead_Beef
     (Storage_Address          : System.Address;
      Size_In_Storage_Elements : Storage_Count);
   --  Set the contents of the memory block pointed to by Storage_Address to
   --  the 16#DEADBEEF# pattern. If Size_In_Storage_Elements is not a multiple
   --  of the length of this pattern, the last instance may be partial.

   procedure Free_Physically (Pool : in out Debug_Pool);
   --  Start to physically release some memory to the system, until the amount
   --  of logically (but not physically) freed memory is lower than the
   --  expected amount in Pool.

   procedure Allocate_End;
   procedure Deallocate_End;
   procedure Dereference_End;
   --  These procedures are used as markers when computing the stacktraces,
   --  so that addresses in the debug pool itself are not reported to the user.

   Code_Address_For_Allocate_End    : System.Address;
   Code_Address_For_Deallocate_End  : System.Address;
   Code_Address_For_Dereference_End : System.Address;
   --  Taking the address of the above procedures will not work on some
   --  architectures (HPUX and VMS for instance). Thus we do the same thing
   --  that is done in a-except.adb, and get the address of labels instead

   procedure Skip_Levels
     (Depth               : Natural;
      Trace               : Tracebacks_Array;
      Start               : out Natural;
      Len                 : in out Natural;
      Ignored_Frame_Start : System.Address;
      Ignored_Frame_End   : System.Address);
   --  Set Start .. Len to the range of values from Trace that should be output
   --  to the user. This range of values exludes any address prior to the first
   --  one in Ignored_Frame_Start .. Ignored_Frame_End (basically addresses
   --  internal to this package). Depth is the number of levels that the user
   --  is interested in.

   ---------------
   -- Header_Of --
   ---------------

   function Header_Of (Address : System.Address)
      return Allocation_Header_Access
   is
      function Convert is new Ada.Unchecked_Conversion
        (System.Address, Allocation_Header_Access);
   begin
      return Convert (Address - Header_Offset);
   end Header_Of;

   --------------
   -- Set_Next --
   --------------

   procedure Set_Next
     (E    : Traceback_Htable_Elem_Ptr;
      Next : Traceback_Htable_Elem_Ptr)
   is
   begin
      E.Next := Next;
   end Set_Next;

   ----------
   -- Next --
   ----------

   function Next
     (E : Traceback_Htable_Elem_Ptr) return Traceback_Htable_Elem_Ptr is
   begin
      return E.Next;
   end Next;

   -----------
   -- Equal --
   -----------

   function Equal (K1, K2 : Tracebacks_Array_Access) return Boolean is
      use Ada.Exceptions.Traceback;
   begin
      return K1.all = K2.all;
   end Equal;

   -------------
   -- Get_Key --
   -------------

   function Get_Key
     (E : Traceback_Htable_Elem_Ptr) return Tracebacks_Array_Access
   is
   begin
      return E.Traceback;
   end Get_Key;

   ----------
   -- Hash --
   ----------

   function Hash (T : Tracebacks_Array_Access) return Header is
      Result : Integer_Address := 0;

   begin
      for X in T'Range loop
         Result := Result + To_Integer (PC_For (T (X)));
      end loop;

      return Header (1 + Result mod Integer_Address (Header'Last));
   end Hash;

   --------------
   -- Put_Line --
   --------------

   procedure Put_Line
     (Depth               : Natural;
      Traceback           : Tracebacks_Array_Access;
      Ignored_Frame_Start : System.Address := System.Null_Address;
      Ignored_Frame_End   : System.Address := System.Null_Address)
   is
      procedure Print (Tr : Tracebacks_Array);
      --  Print the traceback to standard_output

      -----------
      -- Print --
      -----------

      procedure Print (Tr : Tracebacks_Array) is
      begin
         for J in Tr'Range loop
            Put ("0x" & Address_Image (PC_For (Tr (J))) & ' ');
         end loop;
         Put (ASCII.LF);
      end Print;

   --  Start of processing for Put_Line

   begin
      if Traceback = null then
         declare
            Tr  : aliased Tracebacks_Array (1 .. Depth + Max_Ignored_Levels);
            Start, Len : Natural;

         begin
            Call_Chain (Tr, Len);
            Skip_Levels (Depth, Tr, Start, Len,
                         Ignored_Frame_Start, Ignored_Frame_End);
            Print (Tr (Start .. Len));
         end;

      else
         Print (Traceback.all);
      end if;
   end Put_Line;

   -----------------
   -- Skip_Levels --
   -----------------

   procedure Skip_Levels
     (Depth               : Natural;
      Trace               : Tracebacks_Array;
      Start               : out Natural;
      Len                 : in out Natural;
      Ignored_Frame_Start : System.Address;
      Ignored_Frame_End   : System.Address)
   is
   begin
      Start := Trace'First;

      while Start <= Len
        and then (PC_For (Trace (Start)) < Ignored_Frame_Start
                    or else PC_For (Trace (Start)) > Ignored_Frame_End)
      loop
         Start := Start + 1;
      end loop;

      Start := Start + 1;

      --  Just in case: make sure we have a traceback even if Ignore_Till
      --  wasn't found.

      if Start > Len then
         Start := 1;
      end if;

      if Len - Start + 1 > Depth then
         Len := Depth + Start - 1;
      end if;
   end Skip_Levels;

   ------------------------------
   -- Find_Or_Create_Traceback --
   ------------------------------

   function Find_Or_Create_Traceback
     (Pool                : Debug_Pool;
      Kind                : Traceback_Kind;
      Size                : Storage_Count;
      Ignored_Frame_Start : System.Address;
      Ignored_Frame_End   : System.Address) return Traceback_Htable_Elem_Ptr
   is
   begin
      if Pool.Stack_Trace_Depth = 0 then
         return null;
      end if;

      declare
         Trace : aliased Tracebacks_Array
                  (1 .. Integer (Pool.Stack_Trace_Depth) + Max_Ignored_Levels);
         Len, Start   : Natural;
         Elem  : Traceback_Htable_Elem_Ptr;

      begin
         Call_Chain (Trace, Len);
         Skip_Levels (Pool.Stack_Trace_Depth, Trace, Start, Len,
                      Ignored_Frame_Start, Ignored_Frame_End);

         --  Check if the traceback is already in the table

         Elem :=
           Backtrace_Htable.Get (Trace (Start .. Len)'Unrestricted_Access);

         --  If not, insert it

         if Elem = null then
            Elem := new Traceback_Htable_Elem'
              (Traceback => new Tracebacks_Array'(Trace (Start .. Len)),
               Count     => 1,
               Kind      => Kind,
               Total     => Byte_Count (Size),
               Next      => null);
            Backtrace_Htable.Set (Elem);

         else
            Elem.Count := Elem.Count + 1;
            Elem.Total := Elem.Total + Byte_Count (Size);
         end if;

         return Elem;
      end;
   end Find_Or_Create_Traceback;

   --------------
   -- Is_Valid --
   --------------

   function Is_Valid (Storage : System.Address) return Boolean is
      Offset : constant Storage_Offset :=
                 (Storage - Edata) / Default_Alignment;
      Bit : constant Byte := 2 ** Natural (Offset mod System.Storage_Unit);
   begin
      return (Storage mod Default_Alignment) = 0
        and then Offset >= 0
        and then Offset < Valid_Blocks_Size * Storage_Unit
        and then (Valid_Blocks (Offset / Storage_Unit) and Bit) /= 0;
   end Is_Valid;

   ---------------
   -- Set_Valid --
   ---------------

   procedure Set_Valid (Storage : System.Address; Value : Boolean) is
      Offset : Storage_Offset;
      Bit    : Byte;
      Bytes  : Storage_Offset;
      Tmp    : constant Table_Ptr := Valid_Blocks;

      Edata_Align : constant Storage_Offset :=
                      Default_Alignment * Storage_Unit;

      procedure Memset (A : Address; C : Integer; N : size_t);
      pragma Import (C, Memset, "memset");

      procedure Memmove (Dest, Src : Address; N : size_t);
      pragma Import (C, Memmove, "memmove");

   begin
      --  Allocate, or reallocate, the valid blocks table as needed. We start
      --  with a size big enough to handle Initial_Memory_Size bytes of memory,
      --  to avoid too many reallocations. The table will typically be around
      --  16Mb in that case, which is still small enough.

      if Valid_Blocks_Size = 0 then
         Valid_Blocks_Size := (Initial_Memory_Size / Default_Alignment)
                                                      / Storage_Unit;
         Valid_Blocks := To_Pointer (Alloc (size_t (Valid_Blocks_Size)));
         Edata := Storage;

         --  Reset the memory using memset, which is much faster than the
         --  standard Ada code with "when others"

         Memset (Valid_Blocks.all'Address, 0, size_t (Valid_Blocks_Size));
      end if;

      --  First case : the new address is outside of the current scope of
      --  Valid_Blocks, before the current start address. We need to reallocate
      --  the table accordingly. This should be a rare occurence, since in most
      --  cases, the first allocation will also have the lowest address. But
      --  there is no garantee...

      if Storage < Edata then

         --  The difference between the new Edata and the current one must be
         --  a multiple of Default_Alignment * Storage_Unit, so that the bit
         --  representing an address in Valid_Blocks are kept the same.

         Offset := ((Edata - Storage) / Edata_Align + 1) * Edata_Align;
         Offset := Offset / Default_Alignment;
         Bytes  := Offset / Storage_Unit;
         Valid_Blocks :=
           To_Pointer (Alloc (Size => size_t (Valid_Blocks_Size + Bytes)));
         Memmove (Dest => Valid_Blocks.all'Address + Bytes,
                  Src  => Tmp.all'Address,
                  N    => size_t (Valid_Blocks_Size));
         Memset (A => Valid_Blocks.all'Address,
                 C => 0,
                 N => size_t (Bytes));
         Free (Tmp.all'Address);
         Valid_Blocks_Size := Valid_Blocks_Size + Bytes;

         --  Take into the account the new start address

         Edata := Storage - Edata_Align + (Edata - Storage) mod Edata_Align;
      end if;

      --  Second case : the new address is outside of the current scope of
      --  Valid_Blocks, so we have to grow the table as appropriate.

      --  Note: it might seem more natural for the following statement to
      --  be written:

      --      Offset := (Storage - Edata) / Default_Alignment;

      --  but that won't work since Storage_Offset is signed, and it is
      --  possible to subtract a small address from a large address and
      --  get a negative value. This may seem strange, but it is quite
      --  specifically allowed in the RM, and is what most implementations
      --  including GNAT actually do. Hence the conversion to Integer_Address
      --  which is a full range modular type, not subject to this glitch.

      Offset := Storage_Offset ((To_Integer (Storage) - To_Integer (Edata)) /
                                              Default_Alignment);

      if Offset >= Valid_Blocks_Size * System.Storage_Unit then
         Bytes := Valid_Blocks_Size;
         loop
            Bytes := 2 * Bytes;
            exit when Offset <= Bytes * System.Storage_Unit;
         end loop;

         Valid_Blocks := To_Pointer
           (Realloc (Ptr  => Valid_Blocks.all'Address,
                     Size => size_t (Bytes)));
         Memset
           (Valid_Blocks.all'Address + Valid_Blocks_Size,
            0,
            size_t (Bytes - Valid_Blocks_Size));
         Valid_Blocks_Size := Bytes;
      end if;

      Bit    := 2 ** Natural (Offset mod System.Storage_Unit);
      Bytes  := Offset / Storage_Unit;

      --  Then set the value as valid

      if Value then
         Valid_Blocks (Bytes) := Valid_Blocks (Bytes) or Bit;
      else
         Valid_Blocks (Bytes) := Valid_Blocks (Bytes) and (not Bit);
      end if;
   end Set_Valid;

   --------------
   -- Allocate --
   --------------

   procedure Allocate
     (Pool                     : in out Debug_Pool;
      Storage_Address          : out Address;
      Size_In_Storage_Elements : Storage_Count;
      Alignment                : Storage_Count)
   is
      pragma Unreferenced (Alignment);
      --  Ignored, we always force 'Default_Alignment

      type Local_Storage_Array is new Storage_Array
        (1 .. Size_In_Storage_Elements + Minimum_Allocation);

      type Ptr is access Local_Storage_Array;
      --  On some systems, we might want to physically protect pages
      --  against writing when they have been freed (of course, this is
      --  expensive in terms of wasted memory). To do that, all we should
      --  have to do it to set the size of this array to the page size.
      --  See mprotect().

      P : Ptr;

      Current : Byte_Count;
      Trace   : Traceback_Htable_Elem_Ptr;

   begin
      <<Allocate_Label>>
      Lock_Task.all;

      --  If necessary, start physically releasing memory. The reason this is
      --  done here, although Pool.Logically_Deallocated has not changed above,
      --  is so that we do this only after a series of deallocations (e.g a
      --  loop that deallocates a big array). If we were doing that in
      --  Deallocate, we might be physically freeing memory several times
      --  during the loop, which is expensive.

      if Pool.Logically_Deallocated >
        Byte_Count (Pool.Maximum_Logically_Freed_Memory)
      then
         Free_Physically (Pool);
      end if;

      --  Use standard (ie through malloc) allocations. This automatically
      --  raises Storage_Error if needed. We also try once more to physically
      --  release memory, so that even marked blocks, in the advanced scanning,
      --  are freed.

      begin
         P := new Local_Storage_Array;

      exception
         when Storage_Error =>
            Free_Physically (Pool);
            P := new Local_Storage_Array;
      end;

      Storage_Address :=
        System.Null_Address + Default_Alignment
          * (((P.all'Address + Default_Alignment - 1) - System.Null_Address)
             / Default_Alignment)
        + Header_Offset;

      pragma Assert ((Storage_Address - System.Null_Address)
                     mod Default_Alignment = 0);
      pragma Assert (Storage_Address + Size_In_Storage_Elements
                     <= P.all'Address + P'Length);

      Trace := Find_Or_Create_Traceback
        (Pool, Alloc, Size_In_Storage_Elements,
         Allocate_Label'Address, Code_Address_For_Allocate_End);

      pragma Warnings (Off);
      --  Turn warning on alignment for convert call off. We know that in
      --  fact this conversion is safe since P itself is always aligned on
      --  Default_Alignment.

      Header_Of (Storage_Address).all :=
        (Allocation_Address => P.all'Address,
         Alloc_Traceback    => Trace,
         Dealloc_Traceback  => To_Traceback (null),
         Next               => Pool.First_Used_Block,
         Block_Size         => Size_In_Storage_Elements);

      pragma Warnings (On);

      --  Link this block in the list of used blocks. This will be used to list
      --  memory leaks in Print_Info, and for the advanced schemes of
      --  Physical_Free, where we want to traverse all allocated blocks and
      --  search for possible references.

      --  We insert in front, since most likely we'll be freeing the most
      --  recently allocated blocks first (the older one might stay allocated
      --  for the whole life of the application).

      if Pool.First_Used_Block /= System.Null_Address then
         Header_Of (Pool.First_Used_Block).Dealloc_Traceback :=
           To_Address (Storage_Address);
      end if;

      Pool.First_Used_Block := Storage_Address;

      --  Mark the new address as valid

      Set_Valid (Storage_Address, True);

      --  Update internal data

      Pool.Allocated :=
        Pool.Allocated + Byte_Count (Size_In_Storage_Elements);

      Current := Pool.Allocated -
                   Pool.Logically_Deallocated -
                     Pool.Physically_Deallocated;

      if Current > Pool.High_Water then
         Pool.High_Water := Current;
      end if;

      Unlock_Task.all;

   exception
      when others =>
         Unlock_Task.all;
         raise;
   end Allocate;

   ------------------
   -- Allocate_End --
   ------------------

   --  DO NOT MOVE, this must be right after Allocate. This is similar to
   --  what is done in a-except, so that we can hide the traceback frames
   --  internal to this package

   procedure Allocate_End is
   begin
      <<Allocate_End_Label>>
      Code_Address_For_Allocate_End := Allocate_End_Label'Address;
   end Allocate_End;

   -------------------
   -- Set_Dead_Beef --
   -------------------

   procedure Set_Dead_Beef
     (Storage_Address          : System.Address;
      Size_In_Storage_Elements : Storage_Count)
   is
      Dead_Bytes : constant := 4;

      type Data is mod 2 ** (Dead_Bytes * 8);
      for Data'Size use Dead_Bytes * 8;

      Dead : constant Data := 16#DEAD_BEEF#;

      type Dead_Memory is array
        (1 .. Size_In_Storage_Elements / Dead_Bytes) of Data;
      type Mem_Ptr is access Dead_Memory;

      type Byte is mod 2 ** 8;
      for Byte'Size use 8;

      type Dead_Memory_Bytes is array (0 .. 2) of Byte;
      type Dead_Memory_Bytes_Ptr is access Dead_Memory_Bytes;

      function From_Ptr is new Ada.Unchecked_Conversion
        (System.Address, Mem_Ptr);

      function From_Ptr is new Ada.Unchecked_Conversion
        (System.Address, Dead_Memory_Bytes_Ptr);

      M      : constant Mem_Ptr := From_Ptr (Storage_Address);
      M2     : Dead_Memory_Bytes_Ptr;
      Modulo : constant Storage_Count :=
                 Size_In_Storage_Elements mod Dead_Bytes;
   begin
      M.all := (others => Dead);

      --  Any bytes left (up to three of them)

      if Modulo /= 0 then
         M2 := From_Ptr (Storage_Address + M'Length * Dead_Bytes);

         M2 (0) := 16#DE#;
         if Modulo >= 2 then
            M2 (1) := 16#AD#;

            if Modulo >= 3 then
               M2 (2) := 16#BE#;
            end if;
         end if;
      end if;
   end Set_Dead_Beef;

   ---------------------
   -- Free_Physically --
   ---------------------

   procedure Free_Physically (Pool : in out Debug_Pool) is
      type Byte is mod 256;
      type Byte_Access is access Byte;

      function To_Byte is new Ada.Unchecked_Conversion
        (System.Address, Byte_Access);

      type Address_Access is access System.Address;

      function To_Address_Access is new Ada.Unchecked_Conversion
        (System.Address, Address_Access);

      In_Use_Mark : constant Byte := 16#D#;
      Free_Mark   : constant Byte := 16#F#;

      Total_Freed : Storage_Count := 0;

      procedure Reset_Marks;
      --  Unmark all the logically freed blocks, so that they are considered
      --  for physical deallocation

      procedure Mark
        (H : Allocation_Header_Access; A : System.Address; In_Use : Boolean);
      --  Mark the user data block starting at A. For a block of size zero,
      --  nothing is done. For a block with a different size, the first byte
      --  is set to either "D" (in use) or "F" (free).

      function Marked (A : System.Address) return Boolean;
      --  Return true if the user data block starting at A might be in use
      --  somewhere else

      procedure Mark_Blocks;
      --  Traverse all allocated blocks, and search for possible references
      --  to logically freed blocks. Mark them appropriately

      procedure Free_Blocks (Ignore_Marks : Boolean);
      --  Physically release blocks. Only the blocks that haven't been marked
      --  will be released, unless Ignore_Marks is true.

      -----------------
      -- Free_Blocks --
      -----------------

      procedure Free_Blocks (Ignore_Marks : Boolean) is
         Header   : Allocation_Header_Access;
         Tmp      : System.Address := Pool.First_Free_Block;
         Next     : System.Address;
         Previous : System.Address := System.Null_Address;

      begin
         while Tmp /= System.Null_Address
           and then Total_Freed < Pool.Minimum_To_Free
         loop
            Header := Header_Of (Tmp);

            --  If we know, or at least assume, the block is no longer
            --  reference anywhere, we can free it physically.

            if Ignore_Marks or else not Marked (Tmp) then

               declare
                  pragma Suppress (All_Checks);
                  --  Suppress the checks on this section. If they are overflow
                  --  errors, it isn't critical, and we'd rather avoid a
                  --  Constraint_Error in that case.
               begin
                  --  Note that block_size < zero for freed blocks

                  Pool.Physically_Deallocated :=
                    Pool.Physically_Deallocated -
                      Byte_Count (Header.Block_Size);

                  Pool.Logically_Deallocated :=
                    Pool.Logically_Deallocated +
                      Byte_Count (Header.Block_Size);

                  Total_Freed := Total_Freed - Header.Block_Size;
               end;

               Next := Header.Next;
               System.Memory.Free (Header.Allocation_Address);
               Set_Valid (Tmp, False);

               --  Remove this block from the list

               if Previous = System.Null_Address then
                  Pool.First_Free_Block := Next;
               else
                  Header_Of (Previous).Next := Next;
               end if;

               Tmp  := Next;

            else
               Previous := Tmp;
               Tmp := Header.Next;
            end if;
         end loop;
      end Free_Blocks;

      ----------
      -- Mark --
      ----------

      procedure Mark
        (H      : Allocation_Header_Access;
         A      : System.Address;
         In_Use : Boolean)
      is
      begin
         if H.Block_Size /= 0 then
            if In_Use then
               To_Byte (A).all := In_Use_Mark;
            else
               To_Byte (A).all := Free_Mark;
            end if;
         end if;
      end Mark;

      -----------------
      -- Mark_Blocks --
      -----------------

      procedure Mark_Blocks is
         Tmp      : System.Address := Pool.First_Used_Block;
         Previous : System.Address;
         Last     : System.Address;
         Pointed  : System.Address;
         Header   : Allocation_Header_Access;

      begin
         --  For each allocated block, check its contents. Things that look
         --  like a possible address are used to mark the blocks so that we try
         --  and keep them, for better detection in case of invalid access.
         --  This mechanism is far from being fool-proof: it doesn't check the
         --  stacks of the threads, doesn't check possible memory allocated not
         --  under control of this debug pool. But it should allow us to catch
         --  more cases.

         while Tmp /= System.Null_Address loop
            Previous := Tmp;
            Last     := Tmp + Header_Of (Tmp).Block_Size;
            while Previous < Last loop
               --  ??? Should we move byte-per-byte, or consider that addresses
               --  are always aligned on 4-bytes boundaries ? Let's use the
               --  fastest for now.

               Pointed := To_Address_Access (Previous).all;
               if Is_Valid (Pointed) then
                  Header := Header_Of (Pointed);

                  --  Do not even attempt to mark blocks in use. That would
                  --  screw up the whole application, of course.
                  if Header.Block_Size < 0 then
                     Mark (Header, Pointed, In_Use => True);
                  end if;
               end if;

               Previous := Previous + System.Address'Size;
            end loop;

            Tmp := Header_Of (Tmp).Next;
         end loop;
      end Mark_Blocks;

      ------------
      -- Marked --
      ------------

      function Marked (A : System.Address) return Boolean is
      begin
         return To_Byte (A).all = In_Use_Mark;
      end Marked;

      -----------------
      -- Reset_Marks --
      -----------------

      procedure Reset_Marks is
         Current : System.Address := Pool.First_Free_Block;
         Header  : Allocation_Header_Access;
      begin
         while Current /= System.Null_Address loop
            Header := Header_Of (Current);
            Mark (Header, Current, False);
            Current := Header.Next;
         end loop;
      end Reset_Marks;

   --  Start of processing for Free_Physically

   begin
      Lock_Task.all;

      if Pool.Advanced_Scanning then
         Reset_Marks; --  Reset the mark for each freed block
         Mark_Blocks;
      end if;

      Free_Blocks (Ignore_Marks => not Pool.Advanced_Scanning);

      --  The contract is that we need to free at least Minimum_To_Free bytes,
      --  even if this means freeing marked blocks in the advanced scheme

      if Total_Freed < Pool.Minimum_To_Free
        and then Pool.Advanced_Scanning
      then
         Pool.Marked_Blocks_Deallocated := True;
         Free_Blocks (Ignore_Marks => True);
      end if;

      Unlock_Task.all;

   exception
      when others =>
         Unlock_Task.all;
         raise;
   end Free_Physically;

   ----------------
   -- Deallocate --
   ----------------

   procedure Deallocate
     (Pool                     : in out Debug_Pool;
      Storage_Address          : Address;
      Size_In_Storage_Elements : Storage_Count;
      Alignment                : Storage_Count)
   is
      pragma Unreferenced (Alignment);

      Header   : constant Allocation_Header_Access :=
        Header_Of (Storage_Address);
      Valid    : Boolean;
      Previous : System.Address;

   begin
      <<Deallocate_Label>>
      Lock_Task.all;
      Valid := Is_Valid (Storage_Address);

      if not Valid then
         Unlock_Task.all;
         if Pool.Raise_Exceptions then
            raise Freeing_Not_Allocated_Storage;
         else
            Put ("error: Freeing not allocated storage, at ");
            Put_Line (Pool.Stack_Trace_Depth, null,
                      Deallocate_Label'Address,
                      Code_Address_For_Deallocate_End);
         end if;

      elsif Header.Block_Size < 0 then
         Unlock_Task.all;
         if Pool.Raise_Exceptions then
            raise Freeing_Deallocated_Storage;
         else
            Put ("error: Freeing already deallocated storage, at ");
            Put_Line (Pool.Stack_Trace_Depth, null,
                      Deallocate_Label'Address,
                      Code_Address_For_Deallocate_End);
            Put ("   Memory already deallocated at ");
            Put_Line (0, To_Traceback (Header.Dealloc_Traceback).Traceback);
            Put ("   Memory was allocated at ");
            Put_Line (0, Header.Alloc_Traceback.Traceback);
         end if;

      else
         --  Remove this block from the list of used blocks

         Previous :=
           To_Address (Header_Of (Storage_Address).Dealloc_Traceback);

         if Previous = System.Null_Address then
            Pool.First_Used_Block := Header_Of (Pool.First_Used_Block).Next;

            if Pool.First_Used_Block /= System.Null_Address then
               Header_Of (Pool.First_Used_Block).Dealloc_Traceback :=
                 To_Traceback (null);
            end if;

         else
            Header_Of (Previous).Next := Header_Of (Storage_Address).Next;

            if Header_Of (Storage_Address).Next /= System.Null_Address then
               Header_Of
                 (Header_Of (Storage_Address).Next).Dealloc_Traceback :=
                    To_Address (Previous);
            end if;
         end if;

         --  Update the header

         Header.all :=
           (Allocation_Address => Header.Allocation_Address,
            Alloc_Traceback    => Header.Alloc_Traceback,
            Dealloc_Traceback  => To_Traceback
                                    (Find_Or_Create_Traceback
                                       (Pool, Dealloc,
                                        Size_In_Storage_Elements,
                                        Deallocate_Label'Address,
                                        Code_Address_For_Deallocate_End)),
            Next               => System.Null_Address,
            Block_Size         => -Size_In_Storage_Elements);

         if Pool.Reset_Content_On_Free then
            Set_Dead_Beef (Storage_Address, Size_In_Storage_Elements);
         end if;

         Pool.Logically_Deallocated :=
           Pool.Logically_Deallocated +
             Byte_Count (Size_In_Storage_Elements);

         --  Link this free block with the others (at the end of the list, so
         --  that we can start releasing the older blocks first later on).

         if Pool.First_Free_Block = System.Null_Address then
            Pool.First_Free_Block := Storage_Address;
            Pool.Last_Free_Block := Storage_Address;

         else
            Header_Of (Pool.Last_Free_Block).Next := Storage_Address;
            Pool.Last_Free_Block := Storage_Address;
         end if;

         --  Do not physically release the memory here, but in Alloc.
         --  See comment there for details.

         Unlock_Task.all;
      end if;

   exception
      when others =>
         Unlock_Task.all;
         raise;
   end Deallocate;

   --------------------
   -- Deallocate_End --
   --------------------

   --  DO NOT MOVE, this must be right after Deallocate
   --  See Allocate_End

   procedure Deallocate_End is
   begin
      <<Deallocate_End_Label>>
      Code_Address_For_Deallocate_End := Deallocate_End_Label'Address;
   end Deallocate_End;

   -----------------
   -- Dereference --
   -----------------

   procedure Dereference
     (Pool                     : in out Debug_Pool;
      Storage_Address          : Address;
      Size_In_Storage_Elements : Storage_Count;
      Alignment                : Storage_Count)
   is
      pragma Unreferenced (Alignment, Size_In_Storage_Elements);

      Valid   : constant Boolean := Is_Valid (Storage_Address);
      Header  : Allocation_Header_Access;

   begin
      --  Locking policy: we do not do any locking in this procedure. The
      --  tables are only read, not written to, and although a problem might
      --  appear if someone else is modifying the tables at the same time, this
      --  race condition is not intended to be detected by this storage_pool (a
      --  now invalid pointer would appear as valid). Instead, we prefer
      --  optimum performance for dereferences.

      <<Dereference_Label>>

      if not Valid then
         if Pool.Raise_Exceptions then
            raise Accessing_Not_Allocated_Storage;
         else
            Put ("error: Accessing not allocated storage, at ");
            Put_Line (Pool.Stack_Trace_Depth, null,
                      Dereference_Label'Address,
                      Code_Address_For_Dereference_End);
         end if;

      else
         Header := Header_Of (Storage_Address);

         if Header.Block_Size < 0 then
            if Pool.Raise_Exceptions then
               raise Accessing_Deallocated_Storage;
            else
               Put ("error: Accessing deallocated storage, at ");
               Put_Line
                 (Pool.Stack_Trace_Depth, null,
                  Dereference_Label'Address,
                  Code_Address_For_Dereference_End);
               Put ("  First deallocation at ");
               Put_Line (0, To_Traceback (Header.Dealloc_Traceback).Traceback);
               Put ("  Initial allocation at ");
               Put_Line (0, Header.Alloc_Traceback.Traceback);
            end if;
         end if;
      end if;
   end Dereference;

   ---------------------
   -- Dereference_End --
   ---------------------

   --  DO NOT MOVE: this must be right after Dereference
   --  See Allocate_End

   procedure Dereference_End is
   begin
      <<Dereference_End_Label>>
      Code_Address_For_Dereference_End := Dereference_End_Label'Address;
   end Dereference_End;

   ----------------
   -- Print_Info --
   ----------------

   procedure Print_Info
     (Pool          : Debug_Pool;
      Cumulate      : Boolean := False;
      Display_Slots : Boolean := False;
      Display_Leaks : Boolean := False)
   is

      package Backtrace_Htable_Cumulate is new GNAT.HTable.Static_HTable
        (Header_Num => Header,
         Element    => Traceback_Htable_Elem,
         Elmt_Ptr   => Traceback_Htable_Elem_Ptr,
         Null_Ptr   => null,
         Set_Next   => Set_Next,
         Next       => Next,
         Key        => Tracebacks_Array_Access,
         Get_Key    => Get_Key,
         Hash       => Hash,
         Equal      => Equal);
      --  This needs a comment ??? probably some of the ones below do too???

      Data    : Traceback_Htable_Elem_Ptr;
      Elem    : Traceback_Htable_Elem_Ptr;
      Current : System.Address;
      Header  : Allocation_Header_Access;
      K       : Traceback_Kind;

   begin
      Put_Line
        ("Total allocated bytes : " &
         Byte_Count'Image (Pool.Allocated));

      Put_Line
        ("Total logically deallocated bytes : " &
         Byte_Count'Image (Pool.Logically_Deallocated));

      Put_Line
        ("Total physically deallocated bytes : " &
         Byte_Count'Image (Pool.Physically_Deallocated));

      if Pool.Marked_Blocks_Deallocated then
         Put_Line ("Marked blocks were physically deallocated. This is");
         Put_Line ("potentially dangereous, and you might want to run");
         Put_Line ("again with a lower value of Minimum_To_Free");
      end if;

      Put_Line
        ("Current Water Mark: " &
         Byte_Count'Image
          (Pool.Allocated - Pool.Logically_Deallocated
                                   - Pool.Physically_Deallocated));

      Put_Line
        ("High Water Mark: " &
          Byte_Count'Image (Pool.High_Water));

      Put_Line ("");

      if Display_Slots then
         Data := Backtrace_Htable.Get_First;
         while Data /= null loop
            if Data.Kind in Alloc .. Dealloc then
               Elem :=
                 new Traceback_Htable_Elem'
                      (Traceback => new Tracebacks_Array'(Data.Traceback.all),
                       Count     => Data.Count,
                       Kind      => Data.Kind,
                       Total     => Data.Total,
                       Next      => null);
               Backtrace_Htable_Cumulate.Set (Elem);

               if Cumulate then
                  if Data.Kind = Alloc then
                     K := Indirect_Alloc;
                  else
                     K := Indirect_Dealloc;
                  end if;

                  --  Propagate the direct call to all its parents

                  for T in Data.Traceback'First + 1 .. Data.Traceback'Last loop
                     Elem := Backtrace_Htable_Cumulate.Get
                       (Data.Traceback
                          (T .. Data.Traceback'Last)'Unrestricted_Access);

                     --  If not, insert it

                     if Elem = null then
                        Elem := new Traceback_Htable_Elem'
                          (Traceback => new Tracebacks_Array'
                             (Data.Traceback (T .. Data.Traceback'Last)),
                           Count     => Data.Count,
                           Kind      => K,
                           Total     => Data.Total,
                           Next      => null);
                        Backtrace_Htable_Cumulate.Set (Elem);

                        --  Properly take into account that the subprograms
                        --  indirectly called might be doing either allocations
                        --  or deallocations. This needs to be reflected in the
                        --  counts.

                     else
                        Elem.Count := Elem.Count + Data.Count;

                        if K = Elem.Kind then
                           Elem.Total := Elem.Total + Data.Total;

                        elsif Elem.Total > Data.Total then
                           Elem.Total := Elem.Total - Data.Total;

                        else
                           Elem.Kind  := K;
                           Elem.Total := Data.Total - Elem.Total;
                        end if;
                     end if;
                  end loop;
               end if;

               Data := Backtrace_Htable.Get_Next;
            end if;
         end loop;

         Put_Line ("List of allocations/deallocations: ");

         Data := Backtrace_Htable_Cumulate.Get_First;
         while Data /= null loop
            case Data.Kind is
               when Alloc            => Put ("alloc (count:");
               when Indirect_Alloc   => Put ("indirect alloc (count:");
               when Dealloc          => Put ("free  (count:");
               when Indirect_Dealloc => Put ("indirect free  (count:");
            end case;

            Put (Natural'Image (Data.Count) & ", total:" &
                 Byte_Count'Image (Data.Total) & ") ");

            for T in Data.Traceback'Range loop
               Put ("0x" & Address_Image (PC_For (Data.Traceback (T))) & ' ');
            end loop;

            Put_Line ("");

            Data := Backtrace_Htable_Cumulate.Get_Next;
         end loop;

         Backtrace_Htable_Cumulate.Reset;
      end if;

      if Display_Leaks then
         Put_Line ("");
         Put_Line ("List of not deallocated blocks:");

         --  Do not try to group the blocks with the same stack traces
         --  together. This is done by the gnatmem output.

         Current := Pool.First_Used_Block;
         while Current /= System.Null_Address loop
            Header := Header_Of (Current);

            Put ("Size: " & Storage_Count'Image (Header.Block_Size) & " at: ");

            for T in Header.Alloc_Traceback.Traceback'Range loop
               Put ("0x" & Address_Image
                      (PC_For (Header.Alloc_Traceback.Traceback (T))) & ' ');
            end loop;

            Put_Line ("");
            Current := Header.Next;
         end loop;
      end if;
   end Print_Info;

   ------------------
   -- Storage_Size --
   ------------------

   function Storage_Size (Pool : Debug_Pool) return Storage_Count is
      pragma Unreferenced (Pool);
   begin
      return Storage_Count'Last;
   end Storage_Size;

   ---------------
   -- Configure --
   ---------------

   procedure Configure
     (Pool                           : in out Debug_Pool;
      Stack_Trace_Depth              : Natural := Default_Stack_Trace_Depth;
      Maximum_Logically_Freed_Memory : SSC     := Default_Max_Freed;
      Minimum_To_Free                : SSC     := Default_Min_Freed;
      Reset_Content_On_Free          : Boolean := Default_Reset_Content;
      Raise_Exceptions               : Boolean := Default_Raise_Exceptions;
      Advanced_Scanning              : Boolean := Default_Advanced_Scanning)
   is
   begin
      Pool.Stack_Trace_Depth              := Stack_Trace_Depth;
      Pool.Maximum_Logically_Freed_Memory := Maximum_Logically_Freed_Memory;
      Pool.Reset_Content_On_Free          := Reset_Content_On_Free;
      Pool.Raise_Exceptions               := Raise_Exceptions;
      Pool.Minimum_To_Free                := Minimum_To_Free;
      Pool.Advanced_Scanning              := Advanced_Scanning;
   end Configure;

   ----------------
   -- Print_Pool --
   ----------------

   procedure Print_Pool (A : System.Address) is
      Storage : constant Address := A;
      Valid   : constant Boolean := Is_Valid (Storage);
      Header  : Allocation_Header_Access;

   begin
      --  We might get Null_Address if the call from gdb was done
      --  incorrectly. For instance, doing a "print_pool(my_var)" passes 0x0,
      --  instead of passing the value of my_var

      if A = System.Null_Address then
         Put_Line ("Memory not under control of the storage pool");
         return;
      end if;

      if not Valid then
         Put_Line ("Memory not under control of the storage pool");

      else
         Header := Header_Of (Storage);
         Put_Line ("0x" & Address_Image (A)
                     & " allocated at:");
         Put_Line (0, Header.Alloc_Traceback.Traceback);

         if To_Traceback (Header.Dealloc_Traceback) /= null then
            Put_Line ("0x" & Address_Image (A)
                      & " logically freed memory, deallocated at:");
            Put_Line (0, To_Traceback (Header.Dealloc_Traceback).Traceback);
         end if;
      end if;
   end Print_Pool;

   -----------------------
   -- Print_Info_Stdout --
   -----------------------

   procedure Print_Info_Stdout
     (Pool          : Debug_Pool;
      Cumulate      : Boolean := False;
      Display_Slots : Boolean := False;
      Display_Leaks : Boolean := False)
   is
      procedure Internal is new Print_Info
        (Put_Line => GNAT.IO.Put_Line,
         Put      => GNAT.IO.Put);
   begin
      Internal (Pool, Cumulate, Display_Slots, Display_Leaks);
   end Print_Info_Stdout;

   ------------------
   -- Dump_Gnatmem --
   ------------------

   procedure Dump_Gnatmem (Pool : Debug_Pool; File_Name : String) is
      type File_Ptr is new System.Address;

      function fopen (Path : String; Mode : String) return File_Ptr;
      pragma Import (C, fopen);

      procedure fwrite
        (Ptr    : System.Address;
         Size   : size_t;
         Nmemb  : size_t;
         Stream : File_Ptr);

      procedure fwrite
        (Str    : String;
         Size   : size_t;
         Nmemb  : size_t;
         Stream : File_Ptr);
      pragma Import (C, fwrite);

      procedure fputc (C : Integer; Stream : File_Ptr);
      pragma Import (C, fputc);

      procedure fclose (Stream : File_Ptr);
      pragma Import (C, fclose);

      Address_Size : constant size_t :=
                       System.Address'Max_Size_In_Storage_Elements;
      --  Size in bytes of a pointer

      File        : File_Ptr;
      Current     : System.Address;
      Header      : Allocation_Header_Access;
      Actual_Size : size_t;
      Num_Calls   : Integer;
      Tracebk     : Tracebacks_Array_Access;

   begin
      File := fopen (File_Name & ASCII.NUL, "wb" & ASCII.NUL);
      fwrite ("GMEM DUMP" & ASCII.LF, 10, 1, File);

      --  List of not deallocated blocks (see Print_Info)

      Current := Pool.First_Used_Block;
      while Current /= System.Null_Address loop
         Header := Header_Of (Current);

         Actual_Size := size_t (Header.Block_Size);
         Tracebk := Header.Alloc_Traceback.Traceback;
         Num_Calls := Tracebk'Length;

         --  (Code taken from memtrack.adb in GNAT's sources)

         --  Logs allocation call using the format:

         --   'A' <mem addr> <size chunk> <len backtrace> <addr1> ... <addrn>

         fputc (Character'Pos ('A'), File);
         fwrite (Current'Address, Address_Size, 1, File);
         fwrite (Actual_Size'Address, size_t'Max_Size_In_Storage_Elements, 1,
                 File);
         fwrite (Num_Calls'Address, Integer'Max_Size_In_Storage_Elements, 1,
                 File);

         for J in Tracebk'First .. Tracebk'First + Num_Calls - 1 loop
            declare
               Ptr : System.Address := PC_For (Tracebk (J));
            begin
               fwrite (Ptr'Address, Address_Size, 1, File);
            end;
         end loop;

         Current := Header.Next;
      end loop;

      fclose (File);
   end Dump_Gnatmem;

begin
   Allocate_End;
   Deallocate_End;
   Dereference_End;
end GNAT.Debug_Pools;
