;; ARM VFP coprocessor Machine Description
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
;; Copyright (C) 2003, 2005, 2006, 2007 Free Software Foundation, Inc.
;; Written by CodeSourcery, LLC.
;;
;; This file is part of GCC.
;;
;; GCC is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;;
;; GCC is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING.  If not, write to the Free
;; Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
;; 02110-1301, USA.  */

;; Additional register numbers
(define_constants
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  [(VFPCC_REGNUM 127)]
)
;; ALQAAHIRA LOCAL begin v7 support. Merge from Codesourcery

;; The VFP "type" attributes differ from those used in the FPA model.
;; ffarith	Fast floating point insns, e.g. abs, neg, cpy, cmp.
;; farith	Most arithmetic insns.
;; fmuls	Single precision multiply.
;; fmuld	Double precision multiply.
;; fmacs	Single precision multiply-accumulate.
;; fmacd	Double precision multiply-accumulate.
;; fdivs	Single precision sqrt or division.
;; fdivd	Double precision sqrt or division.
;; f_flag	fmstat operation
;; f_load[sd]	Floating point load from memory.
;; f_store[sd]	Floating point store to memory.
;; f_2_r	Transfer vfp to arm reg.
;; r_2_f	Transfer arm to vfp reg.
;; f_cvt	Convert floating<->integral

;; ALQAAHIRA LOCAL end v7 support. Merge from Codesourcery
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
;; SImode moves
;; ??? For now do not allow loading constants into vfp regs.  This causes
;; problems because small constants get converted into adds.
(define_insn "*arm_movsi_vfp"
  [(set (match_operand:SI 0 "nonimmediate_operand" "=r,r,r,r ,m,*t,r,*t,*t, *Uv")
      (match_operand:SI 1 "general_operand"	   "rI,K,N,mi,r,r,*t,*t,*Uvi,*t"))]
  "TARGET_ARM && TARGET_VFP && TARGET_HARD_FLOAT
   && (   s_register_operand (operands[0], SImode)
       || s_register_operand (operands[1], SImode))"
  "*
  switch (which_alternative)
    {
    case 0:
      return \"mov%?\\t%0, %1\";
    case 1:
      return \"mvn%?\\t%0, #%B1\";
    case 2:
      return \"movw%?\\t%0, %1\";
    case 3:
      return \"ldr%?\\t%0, %1\";
    case 4:
      return \"str%?\\t%1, %0\";
    case 5:
      return \"fmsr%?\\t%0, %1\\t%@ int\";
    case 6:
      return \"fmrs%?\\t%0, %1\\t%@ int\";
    case 7:
      return \"fcpys%?\\t%0, %1\\t%@ int\";
    case 8: case 9:
      return output_move_vfp (operands);
    default:
      gcc_unreachable ();
    }
  "
  [(set_attr "predicable" "yes")
   (set_attr "type" "*,*,*,load1,store1,r_2_f,f_2_r,ffarith,f_loads,f_stores")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "insn" "mov,mvn,mov,*,*,*,*,*,*,*")
   (set_attr "pool_range"     "*,*,*,4096,*,*,*,*,1020,*")
   (set_attr "neg_pool_range" "*,*,*,4084,*,*,*,*,1008,*")]
)

(define_insn "*thumb2_movsi_vfp"
  [(set (match_operand:SI 0 "nonimmediate_operand" "=r,r,r,r,m,*t,r,*t,*t, *Uv")
      (match_operand:SI 1 "general_operand"	   "rI,K,N,mi,r,r,*t,*t,*Uvi,*t"))]
  "TARGET_THUMB2 && TARGET_VFP && TARGET_HARD_FLOAT
   && (   s_register_operand (operands[0], SImode)
       || s_register_operand (operands[1], SImode))"
  "*
  switch (which_alternative)
    {
    case 0:
      return \"mov%?\\t%0, %1\";
    case 1:
      return \"mvn%?\\t%0, #%B1\";
    case 2:
      return \"movw%?\\t%0, %1\";
    case 3:
      return \"ldr%?\\t%0, %1\";
    case 4:
      return \"str%?\\t%1, %0\";
    case 5:
      return \"fmsr%?\\t%0, %1\\t%@ int\";
    case 6:
      return \"fmrs%?\\t%0, %1\\t%@ int\";
    case 7:
      return \"fcpys%?\\t%0, %1\\t%@ int\";
    case 8: case 9:
      return output_move_vfp (operands);
    default:
      gcc_unreachable ();
    }
  "
  [(set_attr "predicable" "yes")
   (set_attr "type" "*,*,*,load1,store1,r_2_f,f_2_r,ffarith,f_load,f_store")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "insn" "mov,mvn,mov,*,*,*,*,*,*,*")
   (set_attr "pool_range"     "*,*,*,4096,*,*,*,*,1020,*")
   (set_attr "neg_pool_range" "*,*,*,   0,*,*,*,*,1008,*")]
)


;; DImode moves

(define_insn "*arm_movdi_vfp"
  [(set (match_operand:DI 0 "nonimmediate_di_operand" "=r, r,m,w,r,w,w, Uv")
	(match_operand:DI 1 "di_operand"              "rIK,mi,r,r,w,w,Uvi,w"))]
  "TARGET_ARM && TARGET_HARD_FLOAT && TARGET_VFP
   && (   register_operand (operands[0], DImode)
       || register_operand (operands[1], DImode))"
  "*
  switch (which_alternative)
    {
    case 0: 
      return \"#\";
    case 1:
    case 2:
      return output_move_double (operands);
    case 3:
      return \"fmdrr%?\\t%P0, %Q1, %R1\\t%@ int\";
    case 4:
      return \"fmrrd%?\\t%Q0, %R0, %P1\\t%@ int\";
    case 5:
      return \"fcpyd%?\\t%P0, %P1\\t%@ int\";
    case 6: case 7:
      return output_move_vfp (operands);
    default:
      gcc_unreachable ();
    }
  "
  [(set_attr "type" "*,load2,store2,r_2_f,f_2_r,ffarith,f_loadd,f_stored")
   (set_attr "length" "8,8,8,4,4,4,4,4")
   (set_attr "pool_range"     "*,1020,*,*,*,*,1020,*")
   (set_attr "neg_pool_range" "*,1008,*,*,*,*,1008,*")]
)

(define_insn "*thumb2_movdi_vfp"
  [(set (match_operand:DI 0 "nonimmediate_di_operand" "=r, r,m,w,r,w,w, Uv")
	(match_operand:DI 1 "di_operand"              "rIK,mi,r,r,w,w,Uvi,w"))]
  "TARGET_THUMB2 && TARGET_HARD_FLOAT && TARGET_VFP"
  "*
  switch (which_alternative)
    {
    case 0: case 1: case 2:
      return (output_move_double (operands));
    case 3:
      return \"fmdrr%?\\t%P0, %Q1, %R1\\t%@ int\";
    case 4:
      return \"fmrrd%?\\t%Q0, %R0, %P1\\t%@ int\";
    case 5:
      return \"fcpyd%?\\t%P0, %P1\\t%@ int\";
    case 6: case 7:
      return output_move_vfp (operands);
    default:
      abort ();
    }
  "
  [(set_attr "type" "*,load2,store2,r_2_f,f_2_r,ffarith,f_load,f_store")
   (set_attr "length" "8,8,8,4,4,4,4,4")
   (set_attr "pool_range"     "*,4096,*,*,*,*,1020,*")
   (set_attr "neg_pool_range" "*,   0,*,*,*,*,1008,*")]
)


;; SFmode moves
;; Disparage the w<->r cases because reloading an invalid address is
;; preferable to loading the value via integer registers.

(define_insn "*movsf_vfp"
  [(set (match_operand:SF 0 "nonimmediate_operand" "=t,?r,t ,t  ,Uv,r ,m,t,r")
	(match_operand:SF 1 "general_operand"	   " ?r,t,Dv,UvE,t, mE,r,t,r"))]
  "TARGET_ARM && TARGET_HARD_FLOAT && TARGET_VFP
   && (   s_register_operand (operands[0], SFmode)
       || s_register_operand (operands[1], SFmode))"
  "*
  switch (which_alternative)
    {
    case 0:
      return \"fmsr%?\\t%0, %1\";
    case 1:
      return \"fmrs%?\\t%0, %1\";
    case 2:
      return \"fconsts%?\\t%0, #%G1\";
    case 3: case 4:
      return output_move_vfp (operands);
    case 5:
      return \"ldr%?\\t%0, %1\\t%@ float\";
    case 6:
      return \"str%?\\t%1, %0\\t%@ float\";
    case 7:
      return \"fcpys%?\\t%0, %1\";
    case 8:
      return \"mov%?\\t%0, %1\\t%@ float\";
    default:
      gcc_unreachable ();
    }
  "
  [(set_attr "predicable" "yes")
   (set_attr "type"
     "r_2_f,f_2_r,farith,f_loads,f_stores,load1,store1,ffarith,*")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "insn" "*,*,*,*,*,*,*,*,mov")
   (set_attr "pool_range" "*,*,*,1020,*,4096,*,*,*")
   (set_attr "neg_pool_range" "*,*,*,1008,*,4080,*,*,*")]
)

(define_insn "*thumb2_movsf_vfp"
  [(set (match_operand:SF 0 "nonimmediate_operand" "=t,?r,t, t  ,Uv,r ,m,t,r")
	(match_operand:SF 1 "general_operand"	   " ?r,t,Dv,UvE,t, mE,r,t,r"))]
  "TARGET_THUMB2 && TARGET_HARD_FLOAT && TARGET_VFP
   && (   s_register_operand (operands[0], SFmode)
       || s_register_operand (operands[1], SFmode))"
  "*
  switch (which_alternative)
    {
    case 0:
      return \"fmsr%?\\t%0, %1\";
    case 1:
      return \"fmrs%?\\t%0, %1\";
    case 2:
      return \"fconsts%?\\t%0, #%G1\";
    case 3: case 4:
      return output_move_vfp (operands);
    case 5:
      return \"ldr%?\\t%0, %1\\t%@ float\";
    case 6:
      return \"str%?\\t%1, %0\\t%@ float\";
    case 7:
      return \"fcpys%?\\t%0, %1\";
    case 8:
      return \"mov%?\\t%0, %1\\t%@ float\";
    default:
      gcc_unreachable ();
    }
  "
  [(set_attr "predicable" "yes")
   (set_attr "type"
     "r_2_f,f_2_r,farith,f_load,f_store,load1,store1,ffarith,*")
   (set_attr "pool_range" "*,*,*,1020,*,4092,*,*,*")
   (set_attr "neg_pool_range" "*,*,*,1008,*,0,*,*,*")]
)


;; DFmode moves

(define_insn "*movdf_vfp"
  [(set (match_operand:DF 0 "nonimmediate_soft_df_operand" "=w,?r,w ,r, m,w  ,Uv,w,r")
	(match_operand:DF 1 "soft_df_operand"		   " ?r,w,Dv,mF,r,UvF,w, w,r"))]
  "TARGET_ARM && TARGET_HARD_FLOAT && TARGET_VFP
   && (   register_operand (operands[0], DFmode)
       || register_operand (operands[1], DFmode))"
  "*
  {
    switch (which_alternative)
      {
      case 0:
	return \"fmdrr%?\\t%P0, %Q1, %R1\";
      case 1:
	return \"fmrrd%?\\t%Q0, %R0, %P1\";
      case 2:
        return \"fconstd%?\\t%P0, #%G1\";
      case 3: case 4:
	return output_move_double (operands);
      case 5: case 6:
	return output_move_vfp (operands);
      case 7:
	return \"fcpyd%?\\t%P0, %P1\";
      case 8:
        return \"#\";
      default:
	gcc_unreachable ();
      }
    }
  "
  [(set_attr "type"
     "r_2_f,f_2_r,farith,f_loadd,f_stored,load2,store2,ffarith,*")
   (set_attr "length" "4,4,4,8,8,4,4,4,8")
   (set_attr "pool_range" "*,*,*,1020,*,1020,*,*,*")
   (set_attr "neg_pool_range" "*,*,*,1008,*,1008,*,*,*")]
)

(define_insn "*thumb2_movdf_vfp"
  [(set (match_operand:DF 0 "nonimmediate_soft_df_operand" "=w,?r,w ,r, m,w  ,Uv,w,r")
	(match_operand:DF 1 "soft_df_operand"		   " ?r,w,Dv,mF,r,UvF,w, w,r"))]
  "TARGET_THUMB2 && TARGET_HARD_FLOAT && TARGET_VFP"
  "*
  {
    switch (which_alternative)
      {
      case 0:
	return \"fmdrr%?\\t%P0, %Q1, %R1\";
      case 1:
	return \"fmrrd%?\\t%Q0, %R0, %P1\";
      case 2:
	return \"fconstd%?\\t%P0, #%G1\";
      case 3: case 4: case 8:
	return output_move_double (operands);
      case 5: case 6:
	return output_move_vfp (operands);
      case 7:
	return \"fcpyd%?\\t%P0, %P1\";
      default:
	abort ();
      }
    }
  "
  [(set_attr "type"
     "r_2_f,f_2_r,farith,load2,store2,f_load,f_store,ffarith,*")
   (set_attr "length" "4,4,4,8,8,4,4,4,8")
   (set_attr "pool_range" "*,*,*,4096,*,1020,*,*,*")
   (set_attr "neg_pool_range" "*,*,*,0,*,1008,*,*,*")]
)


;; Conditional move patterns

(define_insn "*movsfcc_vfp"
  [(set (match_operand:SF   0 "s_register_operand" "=t,t,t,t,t,t,?r,?r,?r")
	(if_then_else:SF
	  (match_operator   3 "arm_comparison_operator"
	    [(match_operand 4 "cc_register" "") (const_int 0)])
	  (match_operand:SF 1 "s_register_operand" "0,t,t,0,?r,?r,0,t,t")
	  (match_operand:SF 2 "s_register_operand" "t,0,t,?r,0,?r,t,0,t")))]
  "TARGET_ARM && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   fcpys%D3\\t%0, %2
   fcpys%d3\\t%0, %1
   fcpys%D3\\t%0, %2\;fcpys%d3\\t%0, %1
   fmsr%D3\\t%0, %2
   fmsr%d3\\t%0, %1
   fmsr%D3\\t%0, %2\;fmsr%d3\\t%0, %1
   fmrs%D3\\t%0, %2
   fmrs%d3\\t%0, %1
   fmrs%D3\\t%0, %2\;fmrs%d3\\t%0, %1"
   [(set_attr "conds" "use")
    (set_attr "length" "4,4,8,4,4,8,4,4,8")
    (set_attr "type" "ffarith,ffarith,ffarith,r_2_f,r_2_f,r_2_f,f_2_r,f_2_r,f_2_r")]
)

(define_insn "*thumb2_movsfcc_vfp"
  [(set (match_operand:SF   0 "s_register_operand" "=t,t,t,t,t,t,?r,?r,?r")
	(if_then_else:SF
	  (match_operator   3 "arm_comparison_operator"
	    [(match_operand 4 "cc_register" "") (const_int 0)])
	  (match_operand:SF 1 "s_register_operand" "0,t,t,0,?r,?r,0,t,t")
	  (match_operand:SF 2 "s_register_operand" "t,0,t,?r,0,?r,t,0,t")))]
  "TARGET_THUMB2 && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   it\\t%D3\;fcpys%D3\\t%0, %2
   it\\t%d3\;fcpys%d3\\t%0, %1
   ite\\t%D3\;fcpys%D3\\t%0, %2\;fcpys%d3\\t%0, %1
   it\\t%D3\;fmsr%D3\\t%0, %2
   it\\t%d3\;fmsr%d3\\t%0, %1
   ite\\t%D3\;fmsr%D3\\t%0, %2\;fmsr%d3\\t%0, %1
   it\\t%D3\;fmrs%D3\\t%0, %2
   it\\t%d3\;fmrs%d3\\t%0, %1
   ite\\t%D3\;fmrs%D3\\t%0, %2\;fmrs%d3\\t%0, %1"
   [(set_attr "conds" "use")
    (set_attr "length" "6,6,10,6,6,10,6,6,10")
    (set_attr "type" "ffarith,ffarith,ffarith,r_2_f,r_2_f,r_2_f,f_2_r,f_2_r,f_2_r")]
)
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline

(define_insn "*movdfcc_vfp"
  [(set (match_operand:DF   0 "s_register_operand" "=w,w,w,w,w,w,?r,?r,?r")
	(if_then_else:DF
	  (match_operator   3 "arm_comparison_operator"
	    [(match_operand 4 "cc_register" "") (const_int 0)])
	  (match_operand:DF 1 "s_register_operand" "0,w,w,0,?r,?r,0,w,w")
	  (match_operand:DF 2 "s_register_operand" "w,0,w,?r,0,?r,w,0,w")))]
  "TARGET_ARM && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   fcpyd%D3\\t%P0, %P2
   fcpyd%d3\\t%P0, %P1
   fcpyd%D3\\t%P0, %P2\;fcpyd%d3\\t%P0, %P1
   fmdrr%D3\\t%P0, %Q2, %R2
   fmdrr%d3\\t%P0, %Q1, %R1
   fmdrr%D3\\t%P0, %Q2, %R2\;fmdrr%d3\\t%P0, %Q1, %R1
   fmrrd%D3\\t%Q0, %R0, %P2
   fmrrd%d3\\t%Q0, %R0, %P1
   fmrrd%D3\\t%Q0, %R0, %P2\;fmrrd%d3\\t%Q0, %R0, %P1"
   [(set_attr "conds" "use")
    (set_attr "length" "4,4,8,4,4,8,4,4,8")
    (set_attr "type" "ffarith,ffarith,ffarith,r_2_f,r_2_f,r_2_f,f_2_r,f_2_r,f_2_r")]
)

;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
(define_insn "*thumb2_movdfcc_vfp"
  [(set (match_operand:DF   0 "s_register_operand" "=w,w,w,w,w,w,?r,?r,?r")
	(if_then_else:DF
	  (match_operator   3 "arm_comparison_operator"
	    [(match_operand 4 "cc_register" "") (const_int 0)])
	  (match_operand:DF 1 "s_register_operand" "0,w,w,0,?r,?r,0,w,w")
	  (match_operand:DF 2 "s_register_operand" "w,0,w,?r,0,?r,w,0,w")))]
  "TARGET_THUMB2 && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   it\\t%D3\;fcpyd%D3\\t%P0, %P2
   it\\t%d3\;fcpyd%d3\\t%P0, %P1
   ite\\t%D3\;fcpyd%D3\\t%P0, %P2\;fcpyd%d3\\t%P0, %P1
   it\t%D3\;fmdrr%D3\\t%P0, %Q2, %R2
   it\t%d3\;fmdrr%d3\\t%P0, %Q1, %R1
   ite\\t%D3\;fmdrr%D3\\t%P0, %Q2, %R2\;fmdrr%d3\\t%P0, %Q1, %R1
   it\t%D3\;fmrrd%D3\\t%Q0, %R0, %P2
   it\t%d3\;fmrrd%d3\\t%Q0, %R0, %P1
   ite\\t%D3\;fmrrd%D3\\t%Q0, %R0, %P2\;fmrrd%d3\\t%Q0, %R0, %P1"
   [(set_attr "conds" "use")
    (set_attr "length" "6,6,10,6,6,10,6,6,10")
    (set_attr "type" "ffarith,ffarith,ffarith,r_2_f,r_2_f,r_2_f,f_2_r,f_2_r,f_2_r")]
)


;; Sign manipulation functions

(define_insn "*abssf2_vfp"
  [(set (match_operand:SF	  0 "s_register_operand" "=t")
	(abs:SF (match_operand:SF 1 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fabss%?\\t%0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "ffarith")]
)

(define_insn "*absdf2_vfp"
  [(set (match_operand:DF	  0 "s_register_operand" "=w")
	(abs:DF (match_operand:DF 1 "s_register_operand" "w")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fabsd%?\\t%P0, %P1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "ffarith")]
)

(define_insn "*negsf2_vfp"
  [(set (match_operand:SF	  0 "s_register_operand" "=t,?r")
	(neg:SF (match_operand:SF 1 "s_register_operand" "t,r")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   fnegs%?\\t%0, %1
   eor%?\\t%0, %1, #-2147483648"
  [(set_attr "predicable" "yes")
   (set_attr "type" "ffarith")]
)

(define_insn_and_split "*negdf2_vfp"
  [(set (match_operand:DF	  0 "s_register_operand" "=w,?r,?r")
	(neg:DF (match_operand:DF 1 "s_register_operand" "w,0,r")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   fnegd%?\\t%P0, %P1
   #
   #"
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP && reload_completed
   && arm_general_register_operand (operands[0], DFmode)"
  [(set (match_dup 0) (match_dup 1))]
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "
  if (REGNO (operands[0]) == REGNO (operands[1]))
    {
      operands[0] = gen_highpart (SImode, operands[0]);
      operands[1] = gen_rtx_XOR (SImode, operands[0], GEN_INT (0x80000000));
    }
  else
    {
      rtx in_hi, in_lo, out_hi, out_lo;

      in_hi = gen_rtx_XOR (SImode, gen_highpart (SImode, operands[1]),
			   GEN_INT (0x80000000));
      in_lo = gen_lowpart (SImode, operands[1]);
      out_hi = gen_highpart (SImode, operands[0]);
      out_lo = gen_lowpart (SImode, operands[0]);

      if (REGNO (in_lo) == REGNO (out_hi))
        {
          emit_insn (gen_rtx_SET (SImode, out_lo, in_lo));
	  operands[0] = out_hi;
          operands[1] = in_hi;
        }
      else
        {
          emit_insn (gen_rtx_SET (SImode, out_hi, in_hi));
	  operands[0] = out_lo;
          operands[1] = in_lo;
        }
    }
  "
  [(set_attr "predicable" "yes")
   (set_attr "length" "4,4,8")
   (set_attr "type" "ffarith")]
)


;; Arithmetic insns

(define_insn "*addsf3_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF	   0 "s_register_operand" "=t")
	(plus:SF (match_operand:SF 1 "s_register_operand" "t")
		 (match_operand:SF 2 "s_register_operand" "t")))]
;; ALQAAHIRA LOCAL 6150859 use NEON instructions for SF math
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP && !TARGET_NEON"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fadds%?\\t%0, %1, %2"
  [(set_attr "predicable" "yes")
   (set_attr "type" "farith")]
)

(define_insn "*adddf3_vfp"
  [(set (match_operand:DF	   0 "s_register_operand" "=w")
	(plus:DF (match_operand:DF 1 "s_register_operand" "w")
		 (match_operand:DF 2 "s_register_operand" "w")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "faddd%?\\t%P0, %P1, %P2"
  [(set_attr "predicable" "yes")
   (set_attr "type" "farith")]
)


(define_insn "*subsf3_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF	    0 "s_register_operand" "=t")
	(minus:SF (match_operand:SF 1 "s_register_operand" "t")
		  (match_operand:SF 2 "s_register_operand" "t")))]
;; ALQAAHIRA LOCAL 6150859 use NEON instructions for SF math
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP && !TARGET_NEON"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fsubs%?\\t%0, %1, %2"
  [(set_attr "predicable" "yes")
   (set_attr "type" "farith")]
)

(define_insn "*subdf3_vfp"
  [(set (match_operand:DF	    0 "s_register_operand" "=w")
	(minus:DF (match_operand:DF 1 "s_register_operand" "w")
		  (match_operand:DF 2 "s_register_operand" "w")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fsubd%?\\t%P0, %P1, %P2"
  [(set_attr "predicable" "yes")
   (set_attr "type" "farith")]
)


;; Division insns

(define_insn "*divsf3_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF	  0 "s_register_operand" "+t")
	(div:SF (match_operand:SF 1 "s_register_operand" "t")
		(match_operand:SF 2 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fdivs%?\\t%0, %1, %2"
  [(set_attr "predicable" "yes")
   (set_attr "type" "fdivs")]
)

(define_insn "*divdf3_vfp"
  [(set (match_operand:DF	  0 "s_register_operand" "+w")
	(div:DF (match_operand:DF 1 "s_register_operand" "w")
		(match_operand:DF 2 "s_register_operand" "w")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fdivd%?\\t%P0, %P1, %P2"
  [(set_attr "predicable" "yes")
   (set_attr "type" "fdivd")]
)


;; Multiplication insns

(define_insn "*mulsf3_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF	   0 "s_register_operand" "+t")
	(mult:SF (match_operand:SF 1 "s_register_operand" "t")
		 (match_operand:SF 2 "s_register_operand" "t")))]
;; ALQAAHIRA LOCAL 6150859 use NEON instructions for SF math
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP && !TARGET_NEON"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fmuls%?\\t%0, %1, %2"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmuls")]
)

(define_insn "*muldf3_vfp"
  [(set (match_operand:DF	   0 "s_register_operand" "+w")
	(mult:DF (match_operand:DF 1 "s_register_operand" "w")
		 (match_operand:DF 2 "s_register_operand" "w")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fmuld%?\\t%P0, %P1, %P2"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmuld")]
)


(define_insn "*mulsf3negsf_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF		   0 "s_register_operand" "+t")
	(mult:SF (neg:SF (match_operand:SF 1 "s_register_operand" "t"))
		 (match_operand:SF	   2 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fnmuls%?\\t%0, %1, %2"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmuls")]
)

(define_insn "*muldf3negdf_vfp"
  [(set (match_operand:DF		   0 "s_register_operand" "+w")
	(mult:DF (neg:DF (match_operand:DF 1 "s_register_operand" "w"))
		 (match_operand:DF	   2 "s_register_operand" "w")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fnmuld%?\\t%P0, %P1, %P2"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmuld")]
)


;; Multiply-accumulate insns

;; 0 = 1 * 2 + 0
(define_insn "*mulsf3addsf_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF		    0 "s_register_operand" "=t")
	(plus:SF (mult:SF (match_operand:SF 2 "s_register_operand" "t")
			  (match_operand:SF 3 "s_register_operand" "t"))
		 (match_operand:SF	    1 "s_register_operand" "0")))]
;; ALQAAHIRA LOCAL 6150859 use NEON instructions for SF math
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP && !TARGET_NEON"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fmacs%?\\t%0, %2, %3"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmacs")]
)

(define_insn "*muldf3adddf_vfp"
  [(set (match_operand:DF		    0 "s_register_operand" "=w")
	(plus:DF (mult:DF (match_operand:DF 2 "s_register_operand" "w")
			  (match_operand:DF 3 "s_register_operand" "w"))
		 (match_operand:DF	    1 "s_register_operand" "0")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fmacd%?\\t%P0, %P2, %P3"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmacd")]
)

;; 0 = 1 * 2 - 0
(define_insn "*mulsf3subsf_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF		     0 "s_register_operand" "=t")
	(minus:SF (mult:SF (match_operand:SF 2 "s_register_operand" "t")
			   (match_operand:SF 3 "s_register_operand" "t"))
		  (match_operand:SF	     1 "s_register_operand" "0")))]
;; ALQAAHIRA LOCAL 6150859 use NEON instructions for SF math
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP && !TARGET_NEON"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fmscs%?\\t%0, %2, %3"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmacs")]
)

(define_insn "*muldf3subdf_vfp"
  [(set (match_operand:DF		     0 "s_register_operand" "=w")
	(minus:DF (mult:DF (match_operand:DF 2 "s_register_operand" "w")
			   (match_operand:DF 3 "s_register_operand" "w"))
		  (match_operand:DF	     1 "s_register_operand" "0")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fmscd%?\\t%P0, %P2, %P3"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmacd")]
)

;; 0 = -(1 * 2) + 0
(define_insn "*mulsf3negsfaddsf_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF		     0 "s_register_operand" "=t")
	(minus:SF (match_operand:SF	     1 "s_register_operand" "0")
		  (mult:SF (match_operand:SF 2 "s_register_operand" "t")
			   (match_operand:SF 3 "s_register_operand" "t"))))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fnmacs%?\\t%0, %2, %3"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmacs")]
)

(define_insn "*fmuldf3negdfadddf_vfp"
  [(set (match_operand:DF		     0 "s_register_operand" "=w")
	(minus:DF (match_operand:DF	     1 "s_register_operand" "0")
		  (mult:DF (match_operand:DF 2 "s_register_operand" "w")
			   (match_operand:DF 3 "s_register_operand" "w"))))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fnmacd%?\\t%P0, %P2, %P3"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmacd")]
)


;; 0 = -(1 * 2) - 0
(define_insn "*mulsf3negsfsubsf_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF		      0 "s_register_operand" "=t")
	(minus:SF (mult:SF
		    (neg:SF (match_operand:SF 2 "s_register_operand" "t"))
		    (match_operand:SF	      3 "s_register_operand" "t"))
		  (match_operand:SF	      1 "s_register_operand" "0")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fnmscs%?\\t%0, %2, %3"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  [(set_attr "predicable" "yes")
   (set_attr "type" "fmacs")]
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
)

(define_insn "*muldf3negdfsubdf_vfp"
  [(set (match_operand:DF		      0 "s_register_operand" "=w")
	(minus:DF (mult:DF
		    (neg:DF (match_operand:DF 2 "s_register_operand" "w"))
		    (match_operand:DF	      3 "s_register_operand" "w"))
		  (match_operand:DF	      1 "s_register_operand" "0")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fnmscd%?\\t%P0, %P2, %P3"
  [(set_attr "predicable" "yes")
;; ALQAAHIRA LOCAL v7 support. Merge from Codesourcery
   (set_attr "type" "fmacd")]
)


;; Conversion routines

(define_insn "*extendsfdf2_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:DF		   0 "s_register_operand" "=w")
	(float_extend:DF (match_operand:SF 1 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fcvtds%?\\t%P0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)

(define_insn "*truncdfsf2_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF		   0 "s_register_operand" "=t")
	(float_truncate:SF (match_operand:DF 1 "s_register_operand" "w")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fcvtsd%?\\t%0, %P1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)

(define_insn "*truncsisf2_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SI		  0 "s_register_operand" "=t")
	(fix:SI (fix:SF (match_operand:SF 1 "s_register_operand" "t"))))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "ftosizs%?\\t%0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)

(define_insn "*truncsidf2_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SI		  0 "s_register_operand" "=t")
	(fix:SI (fix:DF (match_operand:DF 1 "s_register_operand" "w"))))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "ftosizd%?\\t%0, %P1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)


(define_insn "fixuns_truncsfsi2"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SI		  0 "s_register_operand" "=t")
	(unsigned_fix:SI (fix:SF (match_operand:SF 1 "s_register_operand" "t"))))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "ftouizs%?\\t%0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)

(define_insn "fixuns_truncdfsi2"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SI		  0 "s_register_operand" "=t")
	(unsigned_fix:SI (fix:DF (match_operand:DF 1 "s_register_operand" "t"))))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "ftouizd%?\\t%0, %P1"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)


(define_insn "*floatsisf2_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF	    0 "s_register_operand" "=t")
	(float:SF (match_operand:SI 1 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fsitos%?\\t%0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)

(define_insn "*floatsidf2_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:DF	    0 "s_register_operand" "=w")
	(float:DF (match_operand:SI 1 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fsitod%?\\t%P0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)


(define_insn "floatunssisf2"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF	    0 "s_register_operand" "=t")
	(unsigned_float:SF (match_operand:SI 1 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fuitos%?\\t%0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)

(define_insn "floatunssidf2"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:DF	    0 "s_register_operand" "=w")
	(unsigned_float:DF (match_operand:SI 1 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fuitod%?\\t%P0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "f_cvt")]
)


;; Sqrt insns.

(define_insn "*sqrtsf2_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (match_operand:SF	   0 "s_register_operand" "=t")
	(sqrt:SF (match_operand:SF 1 "s_register_operand" "t")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "fsqrts%?\\t%0, %1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "fdivs")]
)

(define_insn "*sqrtdf2_vfp"
  [(set (match_operand:DF	   0 "s_register_operand" "=w")
	(sqrt:DF (match_operand:DF 1 "s_register_operand" "w")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fsqrtd%?\\t%P0, %P1"
  [(set_attr "predicable" "yes")
   (set_attr "type" "fdivd")]
)


;; Patterns to split/copy vfp condition flags.

(define_insn "*movcc_vfp"
  [(set (reg CC_REGNUM)
	(reg VFPCC_REGNUM))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "fmstat%?"
  [(set_attr "conds" "set")
   (set_attr "type" "f_flag")]
)

(define_insn_and_split "*cmpsf_split_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (reg:CCFP CC_REGNUM)
	(compare:CCFP (match_operand:SF 0 "s_register_operand"  "t")
		      (match_operand:SF 1 "vfp_compare_operand" "tG")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "#"
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  [(set (reg:CCFP VFPCC_REGNUM)
	(compare:CCFP (match_dup 0)
		      (match_dup 1)))
   (set (reg:CCFP CC_REGNUM)
	(reg:CCFP VFPCC_REGNUM))]
  ""
)

(define_insn_and_split "*cmpsf_trap_split_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (reg:CCFPE CC_REGNUM)
	(compare:CCFPE (match_operand:SF 0 "s_register_operand"  "t")
		       (match_operand:SF 1 "vfp_compare_operand" "tG")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "#"
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  [(set (reg:CCFPE VFPCC_REGNUM)
	(compare:CCFPE (match_dup 0)
		       (match_dup 1)))
   (set (reg:CCFPE CC_REGNUM)
	(reg:CCFPE VFPCC_REGNUM))]
  ""
)

(define_insn_and_split "*cmpdf_split_vfp"
  [(set (reg:CCFP CC_REGNUM)
	(compare:CCFP (match_operand:DF 0 "s_register_operand"  "w")
		      (match_operand:DF 1 "vfp_compare_operand" "wG")))]
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "#"
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  [(set (reg:CCFP VFPCC_REGNUM)
	(compare:CCFP (match_dup 0)
		       (match_dup 1)))
   (set (reg:CCFP CC_REGNUM)
	(reg:CCFPE VFPCC_REGNUM))]
  ""
)

(define_insn_and_split "*cmpdf_trap_split_vfp"
  [(set (reg:CCFPE CC_REGNUM)
	(compare:CCFPE (match_operand:DF 0 "s_register_operand"  "w")
		       (match_operand:DF 1 "vfp_compare_operand" "wG")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "#"
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  [(set (reg:CCFPE VFPCC_REGNUM)
	(compare:CCFPE (match_dup 0)
		       (match_dup 1)))
   (set (reg:CCFPE CC_REGNUM)
	(reg:CCFPE VFPCC_REGNUM))]
  ""
)


;; Comparison patterns

(define_insn "*cmpsf_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (reg:CCFP VFPCC_REGNUM)
	(compare:CCFP (match_operand:SF 0 "s_register_operand"  "t,t")
		      (match_operand:SF 1 "vfp_compare_operand" "t,G")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "@
   fcmps%?\\t%0, %1
   fcmpzs%?\\t%0"
  [(set_attr "predicable" "yes")
   (set_attr "type" "ffarith")]
)

(define_insn "*cmpsf_trap_vfp"
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  [(set (reg:CCFPE VFPCC_REGNUM)
	(compare:CCFPE (match_operand:SF 0 "s_register_operand"  "t,t")
		       (match_operand:SF 1 "vfp_compare_operand" "t,G")))]
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  "@
   fcmpes%?\\t%0, %1
   fcmpezs%?\\t%0"
  [(set_attr "predicable" "yes")
   (set_attr "type" "ffarith")]
)

(define_insn "*cmpdf_vfp"
  [(set (reg:CCFP VFPCC_REGNUM)
	(compare:CCFP (match_operand:DF 0 "s_register_operand"  "w,w")
		      (match_operand:DF 1 "vfp_compare_operand" "w,G")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   fcmpd%?\\t%P0, %P1
   fcmpzd%?\\t%P0"
  [(set_attr "predicable" "yes")
   (set_attr "type" "ffarith")]
)

(define_insn "*cmpdf_trap_vfp"
  [(set (reg:CCFPE VFPCC_REGNUM)
	(compare:CCFPE (match_operand:DF 0 "s_register_operand"  "w,w")
		       (match_operand:DF 1 "vfp_compare_operand" "w,G")))]
;; ALQAAHIRA LOCAL v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "@
   fcmped%?\\t%P0, %P1
   fcmpezd%?\\t%P0"
  [(set_attr "predicable" "yes")
   (set_attr "type" "ffarith")]
)


;; Store multiple insn used in function prologue.

(define_insn "*push_multi_vfp"
  [(match_parallel 2 "multi_register_push"
    [(set (match_operand:BLK 0 "memory_operand" "=m")
	  (unspec:BLK [(match_operand:DF 1 "s_register_operand" "w")]
		      UNSPEC_PUSH_MULT))])]
;; ALQAAHIRA LOCAL begin v7 support. Merge from mainline
  "TARGET_32BIT && TARGET_HARD_FLOAT && TARGET_VFP"
  "* return vfp_output_fstmd (operands);"
;; ALQAAHIRA LOCAL end v7 support. Merge from mainline
  [(set_attr "type" "f_stored")]
)


;; Unimplemented insns:
;; fldm*
;; fstm*
;; fmdhr et al (VFPv1)
;; Support for xD (single precision only) variants.
;; fmrrs, fmsrr
