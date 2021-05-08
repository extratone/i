@ libgcc routines for ARM cpu.
@ Division routines, written by Richard Earnshaw, (rearnsha@armltd.co.uk)

/* ALQAAHIRA LOCAL v7 support. Merge from mainline */
/* Copyright 1995, 1996, 1998, 1999, 2000, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.

This file is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

This file is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */
/* ------------------------------------------------------------------------ */

/* We need to know what prefix to add to function names.  */

#ifndef __USER_LABEL_PREFIX__
#error  __USER_LABEL_PREFIX__ not defined
#endif

/* ANSI concatenation macros.  */

#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

/* Use the right prefix for global labels.  */

#define SYM(x) CONCAT1 (__USER_LABEL_PREFIX__, x)

#ifdef __ELF__
#ifdef __thumb__
#define __PLT__  /* Not supported in Thumb assembler (for now).  */
#else
#define __PLT__ (PLT)
#endif
#define TYPE(x) .type SYM(x),function
#define SIZE(x) .size SYM(x), . - SYM(x)
#define LSYM(x) .x
#else
#define __PLT__
#define TYPE(x)
#define SIZE(x)
#define LSYM(x) x
#endif

/* Function end macros.  Variants for interworking.  */

/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */
#if defined(__ARM_ARCH_3M__) || defined(__ARM_ARCH_4__) \
	|| defined(__ARM_ARCH_4T__)
/* We use __ARM_ARCH__ set to 4 here, but in reality it's any processor with
   long multiply instructions.  That includes v3M.  */
# define __ARM_ARCH__ 4
#endif
	
#if defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5T__) \
	|| defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) \
	|| defined(__ARM_ARCH_5TEJ__)
# define __ARM_ARCH__ 5
#endif

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
	|| defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) \
	|| defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__)
# define __ARM_ARCH__ 6
#endif

#if defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) \
	|| defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__)
# define __ARM_ARCH__ 7
#endif

/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
#ifndef __ARM_ARCH__
#error Unable to determine architecture.
#endif

/* How to return from a function call depends on the architecture variant.  */

#if (__ARM_ARCH__ > 4) || defined(__ARM_ARCH_4T__)

# define RET		bx	lr
# define RETc(x)	bx##x	lr

/* Special precautions for interworking on armv4t.  */
# if (__ARM_ARCH__ == 4)

/* Always use bx, not ldr pc.  */
#  if (defined(__thumb__) || defined(__THUMB_INTERWORK__))
#    define __INTERWORKING__
#   endif /* __THUMB__ || __THUMB_INTERWORK__ */

/* Include thumb stub before arm mode code.  */
#  if defined(__thumb__) && !defined(__THUMB_INTERWORK__)
#   define __INTERWORKING_STUBS__
#  endif /* __thumb__ && !__THUMB_INTERWORK__ */

#endif /* __ARM_ARCH == 4 */

#else

# define RET		mov	pc, lr
# define RETc(x)	mov##x	pc, lr

#endif

.macro	cfi_pop		advance, reg, cfa_offset
#ifdef __ELF__
	.pushsection	.debug_frame
	.byte	0x4		/* DW_CFA_advance_loc4 */
	.4byte	\advance
	.byte	(0xc0 | \reg)	/* DW_CFA_restore */
	.byte	0xe		/* DW_CFA_def_cfa_offset */
	.uleb128 \cfa_offset
	.popsection
#endif
.endm
.macro	cfi_push	advance, reg, offset, cfa_offset
#ifdef __ELF__
	.pushsection	.debug_frame
	.byte	0x4		/* DW_CFA_advance_loc4 */
	.4byte	\advance
	.byte	(0x80 | \reg)	/* DW_CFA_offset */
	.uleb128 (\offset / -4)
	.byte	0xe		/* DW_CFA_def_cfa_offset */
	.uleb128 \cfa_offset
	.popsection
#endif
.endm
.macro cfi_start	start_label, end_label
#ifdef __ELF__
	.pushsection	.debug_frame
LSYM(Lstart_frame):
	.4byte	LSYM(Lend_cie) - LSYM(Lstart_cie) @ Length of CIE
LSYM(Lstart_cie):
        .4byte	0xffffffff	@ CIE Identifier Tag
        .byte	0x1	@ CIE Version
        .ascii	"\0"	@ CIE Augmentation
        .uleb128 0x1	@ CIE Code Alignment Factor
        .sleb128 -4	@ CIE Data Alignment Factor
        .byte	0xe	@ CIE RA Column
        .byte	0xc	@ DW_CFA_def_cfa
        .uleb128 0xd
        .uleb128 0x0

	.align 2
LSYM(Lend_cie):
	.4byte	LSYM(Lend_fde)-LSYM(Lstart_fde)	@ FDE Length
LSYM(Lstart_fde):
	.4byte	LSYM(Lstart_frame)	@ FDE CIE offset
	.4byte	\start_label	@ FDE initial location
	.4byte	\end_label-\start_label	@ FDE address range
	.popsection
#endif
.endm
.macro cfi_end	end_label
#ifdef __ELF__
	.pushsection	.debug_frame
	.align	2
LSYM(Lend_fde):
	.popsection
\end_label:
#endif
.endm

/* APPLE LOCAL begin ARM MACH assembler macros */
#if defined (__INTERWORKING__)
#define RETLDM \
	ldr     lr, [sp], #8 ; \
	bx      lr
/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */
#if definded (__thumb2__)
#define RETLDM1(...) \
	pop     {__VA_ARGS__, lr} ; \
	bx      lr
#define RETLDM2(cond,...) \
	pop##cond       {__VA_ARGS__, lr} ; \
	bx##cond        lr
#else
#define RETLDM1(...) \
	ldmia   sp!, {__VA_ARGS__, lr} ; \
	bx      lr
#define RETLDM2(cond,...) \
	ldm##cond##ia   sp!, {__VA_ARGS__, lr} ; \
	bx##cond        lr
#endif

/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
#define RETLDM_unwind(addr) \
	ldr	lr, [sp], #8 ; \
9:	cfi_pop	9b - addr, 0xe, 0x0 ; \
	bx	lr
#else
#define RETLDM \
	ldr     pc, [sp], #8
/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */
#if defined (__thumb2__)
#define RETLDM1(...) \
	pop   {__VA_ARGS__, pc}
#define RETLDM2(cond,...) \
	pop##cond   {__VA_ARGS__, pc}
#else
#define RETLDM1(...) \
	ldmia   sp!, {__VA_ARGS__, pc}
#define RETLDM2(cond,...) \
	ldm##cond##ia   sp!, {__VA_ARGS__, pc}
#endif
/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
#define RETLDM_unwind(addr) \
	ldr	pc, [sp], #8
#endif

/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */

/* The Unified assembly syntax allows the same code to be assembled for both
   ARM and Thumb-2.  However this is only supported by recent gas, so define
   a set of macros to allow ARM code on older assemblers.  */
#if defined(__thumb2__)
.macro do_it cond, suffix=""
#if defined (__MACH__)
	it$1	$0
#else
        it\suffix       \cond
#endif
.endm
.macro shift1 op, arg0, arg1, arg2
#if defined (__MACH__)
        $0      $1, $2, $3
#else
        \op     \arg0, \arg1, \arg2
#endif
.endm   
#define do_push push
#define do_pop  pop
#define COND(op1, op2, cond) op1 ## op2 ## cond
/* Perform an arithmetic operation with a variable shift operand.  This
   requires two instructions and a scratch register on Thumb-2.  */
.macro shiftop name, dest, src1, src2, shiftop, shiftreg, tmp
#if defined (__MACH__)
        $4  $6, $3, $5
        $0  $1, $2, $6
#else
        \shiftop \tmp, \src2, \shiftreg
        \name \dest, \src1, \tmp
#endif
.endm   
#else
.macro do_it cond, suffix=""
.endm   
.macro shift1 op, arg0, arg1, arg2
#if defined (__MACH__)
        mov     $1, $2, $0 $3
#else
        mov     \arg0, \arg1, \op \arg2
#endif
.endm
#define do_push stmfd sp!,
#define do_pop  ldmfd sp!,
#define COND(op1, op2, cond) op1 ## cond ## op2
.macro shiftop name, dest, src1, src2, shiftop, shiftreg, tmp
#if defined (__MACH__)
        $0 $1, $2, $3, $4 $5
#else
        \name \dest, \src1, \src2, \shiftop \shiftreg
#endif
.endm
#endif



/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
.macro ARM_LDIV0 name
	str	lr, [sp, #-8]!
#if !defined(__MACH__)
98:	cfi_push 98b - __\name, 0xe, -0x8, 0x8
#endif
	bl	SYM (__div0) __PLT__
	mov	r0, #0			@ About as wrong as it could be.
	RETLDM_unwind (8b)
.endm


.macro THUMB_LDIV0 name
	push	{ r1, lr }
#if !defined(__MACH__)
7:	cfi_push 7b - __\name, 0xe, -0x4, 0x8
#endif
	bl	SYM (__div0)
	mov	r0, #0			@ About as wrong as it could be.
#if defined (__INTERWORKING__)
	pop	{ r1, r2 }
	bx	r2
#else
	pop	{ r1, pc }
#endif
.endm

.macro FUNC_END name
#if defined(__MACH__)
	SIZE (__$0)
#else
	SIZE (__\name)
#endif
.endm

.macro DIV_FUNC_END name
#if !defined(__MACH__)
	cfi_start	__\name, LSYM(Lend_div0)
#endif
LSYM(Ldiv0):
#ifdef __thumb__
	THUMB_LDIV0 \name
#else
	ARM_LDIV0 \name
#endif
#if defined(__MACH__)
	FUNC_END $0
#else
	cfi_end	LSYM(Lend_div0)
	FUNC_END \name
#endif
.endm

.macro THUMB_FUNC_START name
#if defined(__MACH__)
	.globl	SYM ($0)
	TYPE	($0)
	.thumb_func
SYM ($0):
#else
	.globl	SYM (\name)
	TYPE	(\name)
	.thumb_func
SYM (\name):
#endif
.endm
/* APPLE LOCAL end ARM MACH assembler */

/* Function start macros.  Variants for ARM and Thumb.  */

#ifdef __thumb__
#define THUMB_FUNC .thumb_func
#define THUMB_CODE .force_thumb
/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */
# if defined(__thumb2__)
#define THUMB_SYNTAX .syntax divided
# else
#define THUMB_SYNTAX
# endif
/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
/* APPLE LOCAL ARM function alignment */
#define FUNC_ALIGN .align 1
#else
#define THUMB_FUNC
#define THUMB_CODE
/* APPLE LOCAL ARM function alignment */
#define FUNC_ALIGN .align 2
/* ALQAAHIRA LOCAL v7 support. Merge from mainline */
#define THUMB_SYNTAX
#endif
	
/* APPLE LOCAL begin ARM MACH assembler */
.macro FUNC_START name
#if defined(__MACH__)
	.text
	.globl SYM (__$0)
	TYPE (__$0)
	FUNC_ALIGN
	THUMB_CODE
	THUMB_FUNC
SYM (__$0):
#else
	.text
	.globl SYM (__\name)
	TYPE (__\name)
	.align 0
	THUMB_CODE
	THUMB_FUNC
SYM (__\name):
#endif
.endm

/* Special function that will always be coded in ARM assembly, even if
   in Thumb-only compilation.  */

/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */
#if defined(__thumb2__)

/* For Thumb-2 we build everything in thumb mode.  */
.macro ARM_FUNC_START name
#if defined(__MACH__)
	FUNC_START $0
#else
       FUNC_START \name
#endif
       .syntax unified
.endm
#define EQUIV .thumb_set
.macro  ARM_CALL name
#if defined(__MACH__)
  bl ___$0
#else
  bl  ___\name
#endif
.endm

#elif defined(__INTERWORKING_STUBS__)

/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
.macro	ARM_FUNC_START name

#if defined(__MACH__)
	FUNC_START $0
#else
	FUNC_START \name
#endif
	bx	pc
	nop
	.arm
/* A hook to tell gdb that we've switched to ARM mode.  Also used to call
   directly from other local arm routines.  */
#if defined(__MACH__)
_L__$0:
#else
_L__\name:		
#endif
.endm
#define EQUIV .thumb_set
/* Branch directly to a function declared with ARM_FUNC_START.
   Must be called in arm mode.  */
.macro  ARM_CALL name
#if defined(__MACH__)
	bl	_L__$0
#else
	bl	_L__\name
#endif
.endm
/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */

#else /* !(__INTERWORKING_STUBS__ || __thumb2__) */

/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
.macro	ARM_FUNC_START name
#if defined(__MACH__)
	.text
	.globl SYM (__$0)
	TYPE (__$0)
	/* APPLE LOCAL ARM function alignment */
	.align 2
	.arm
SYM (__$0):
#else
	.text
	.globl SYM (__\name)
	TYPE (__\name)
	.align 0
	.arm
SYM (__\name):
#endif
.endm
#define EQUIV .set
.macro  ARM_CALL name
#if defined(__MACH__)
	bl	SYM (__$0)
#else
	bl	__\name
#endif
.endm
#endif

#if defined (__thumb__)
#define FUNC_ALIAS(new,old)				  \
	.globl  SYM (__##new)				; \
	.thumb_set	SYM (__##new), SYM (__##old)
#else
#define FUNC_ALIAS(new,old)				  \
	.globl  SYM (__##new)				; \
	.set    SYM (__##new), SYM (__##old)
#endif

#if defined(__INTERWORKING_STUBS__)
#define ARM_FUNC_ALIAS(new,old)				  \
	.globl  SYM (__##new)				; \
	EQUIV   SYM (_##new), SYM (__##old)		; \
	.set    SYM (_L__##new), SYM (_L__##old)
#else
#define ARM_FUNC_ALIAS(new,old)				  \
	.globl  SYM (__##new)				; \
	EQUIV   SYM (__##new), SYM (__##old)
#endif
/* APPLE LOCAL end ARM MACH assembler */

#ifdef __thumb__
/* Register aliases.  */

work		.req	r4	@ XXXX is this safe ?
dividend	.req	r0
divisor		.req	r1
overdone	.req	r2
result		.req	r2
curbit		.req	r3
#endif
#if 0
ip		.req	r12
sp		.req	r13
lr		.req	r14
pc		.req	r15
#endif

/* ------------------------------------------------------------------------ */
/*		Bodies of the division and modulo routines.		    */
/* ------------------------------------------------------------------------ */	
/* APPLE LOCAL begin ARM MACH assembler */
#if __ARM_ARCH__ >= 5 && ! defined (__OPTIMIZE_SIZE__)
#define ARMV5_DIV_LOOP(dividend, divisor, result)		  \
	.set	shift, shift - 1				; \
	cmp	dividend, divisor, lsl #shift			; \
	adc	result, result, result				; \
	subcs	dividend, dividend, divisor, lsl #shift
#define ARM_DIV_BODY(dividend, divisor, result, curbit)	  	  \
	clz	curbit, dividend				; \
	clz	result, divisor					; \
	sub	curbit, result, curbit				; \
	rsbs	curbit, curbit, #31				; \
	addne	curbit, curbit, curbit, lsl #1			; \
	mov	result, #0					; \
	addne	pc, pc, curbit, lsl #2				; \
	nop							; \
	.set	shift, 32					; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)		; \
	ARMV5_DIV_LOOP (dividend, divisor, result)
#else /* __ARM_ARCH__ < 5 || defined (__OPTIMIZE_SIZE__) */
#if __ARM_ARCH__ >= 5
#define ARM_DIV_BODY_P1(dividend, divisor, result, curbit)	  \
	clz	curbit, divisor					; \
	clz	result, dividend				; \
	sub	result, curbit, result				; \
	mov	curbit, #1					; \
	mov	divisor, divisor, lsl result 			; \
	mov	curbit, curbit, lsl result 			; \
	mov	result, #0
#else /* __ARM_ARCH__ < 5 */
#define ARM_DIV_BODY_P1(dividend, divisor, result, curbit)	  \
	/* Initially shift the divisor left 3 bits if possible,	*/; \
	/* set curbit accordingly.  This allows for curbit to be located	*/; \
	/* at the left end of each 4 bit nibbles in the division loop */; \
	/* to save one loop in most cases. 			*/; \
	tst	divisor, #0xe0000000 				; \
	moveq	divisor, divisor, lsl #3			; \
	moveq	curbit, #8					; \
	movne	curbit, #1					; \
								; \
	/* Unless the divisor is very big, shift it up in multiples of */; \
	/* four bits, since this is the amount of unwinding in the main */; \
	/* division loop.  Continue shifting until the divisor is*/; \
	/* larger than the dividend. 				*/; \
1:	cmp	divisor, #0x10000000 				; \
	cmplo	divisor, dividend 				; \
	movlo	divisor, divisor, lsl #4			; \
	movlo	curbit, curbit, lsl #4				; \
	blo	1b						; \
								; \
	/* For very big divisors, we must shift it a bit at a time, or */; \
	/* we will be in danger of overflowing.			*/; \
1:	cmp	divisor, #0x80000000 				; \
	cmplo	divisor, dividend 				; \
	movlo	divisor, divisor, lsl #1			; \
	movlo	curbit, curbit, lsl #1				; \
	blo	1b						; \
								; \
	mov	result, #0
#endif /* __ARM_ARCH__ < 5 */

#define ARM_DIV_BODY(dividend, divisor, result, curbit)		  \
	ARM_DIV_BODY_P1(dividend, divisor, result, curbit)	; \
								; \
	/* Division loop						*/; \
1:	cmp	dividend, divisor 				; \
	subhs	dividend, dividend, divisor 			; \
	orrhs	result,   result,   curbit 			; \
	cmp	dividend, divisor,  lsr #1 			; \
	subhs	dividend, dividend, divisor, lsr #1		; \
	orrhs	result,   result,   curbit,  lsr #1		; \
	cmp	dividend, divisor,  lsr #2 			; \
	subhs	dividend, dividend, divisor, lsr #2		; \
	orrhs	result,   result,   curbit,  lsr #2		; \
	cmp	dividend, divisor,  lsr #3 			; \
	subhs	dividend, dividend, divisor, lsr #3		; \
	orrhs	result,   result,   curbit,  lsr #3		; \
	cmp	dividend, #0			/* Early termination?	*/; \
	movnes	curbit,   curbit,  lsr #4	/* No, any more bits to do?*/; \
	movne	divisor,  divisor, lsr #4			; \
	bne	1b
#endif /* __ARM_ARCH__ < 5 || defined (__OPTIMIZE_SIZE__) */

/* ------------------------------------------------------------------------ */	
#if __ARM_ARCH__ >= 5
#define ARM_DIV2_ORDER(divisor, order)				  \
	clz	order, divisor					; \
	rsb	order, order, #31
#else
#define ARM_DIV2_ORDER(divisor, order)				  \
	cmp	divisor, #(1 << 16) 				; \
	movhs	divisor, divisor, lsr #16			; \
	movhs	order, #16					; \
	movlo	order, #0					; \
								; \
	cmp	divisor, #(1 << 8) 				; \
	movhs	divisor, divisor, lsr #8			; \
	addhs	order, order, #8 				; \
								; \
	cmp	divisor, #(1 << 4) 				; \
	movhs	divisor, divisor, lsr #4			; \
	addhs	order, order, #4 				; \
								; \
	cmp	divisor, #(1 << 2) 				; \
	addhi	order, order, #3 				; \
	addls	order, order, divisor, lsr #1
#endif
/* ------------------------------------------------------------------------ */
#if __ARM_ARCH__ >= 5 && ! defined (__OPTIMIZE_SIZE__)

#define ARMV5_MOD_LOOP(dividend, divisor)			  \
	.set	shift, shift - 1				; \
	cmp	dividend, divisor, lsl #shift			; \
	subcs	dividend, dividend, divisor, lsl #shift
#define ARM_MOD_BODY(dividend, divisor, order, spare)	 	  \
	clz	order, divisor					; \
	clz	spare, dividend					; \
	sub	order, order, spare				; \
	rsbs	order, order, #31				; \
	addne	pc, pc, order, lsl #3				; \
	nop							; \
	.set	shift, 32					; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)			; \
	ARMV5_MOD_LOOP (dividend, divisor)

#else /* __ARM_ARCH__ < 5 || defined (__OPTIMIZE_SIZE__) */
#if __ARM_ARCH__ >= 5

#define ARM_MOD_BODY_P1(dividend, divisor, order, spare)	  \
	clz	order, divisor					; \
	clz	spare, dividend					; \
	sub	order, order, spare				; \
	mov	divisor, divisor, lsl order
	
#else /* __ARM_ARCH__ < 5 */

#define ARM_MOD_BODY_P1(dividend, divisor, order, spare)	  \
	mov	order, #0					; \
								; \
	/* Unless the divisor is very big, shift it up in multiples of */; \
	/* four bits, since this is the amount of unwinding in the main */; \
	/* division loop.  Continue shifting until the divisor is */; \
	/* larger than the dividend.				*/; \
1:	cmp	divisor, #0x10000000				; \
	cmplo	divisor, dividend				; \
	movlo	divisor, divisor, lsl #4			; \
	addlo	order, order, #4				; \
	blo	1b						; \
								; \
	/* For very big divisors, we must shift it a bit at a time, or */; \
	/* we will be in danger of overflowing.			*/; \
1:	cmp	divisor, #0x80000000				; \
	cmplo	divisor, dividend				; \
	movlo	divisor, divisor, lsl #1			; \
	addlo	order, order, #1				; \
	blo	1b

#endif /* __ARM_ARCH__ < 5 */
#define ARM_MOD_BODY(dividend, divisor, order, spare) \
ARM_MOD_BODY_P1(dividend, divisor, order, spare)	  	; \
								; \
	/* Perform all needed substractions to keep only the reminder. */; \
	/* Do comparisons in batch of 4 first.			*/; \
	subs	order, order, #3		/* yes, 3 is intended here */; \
	blt	2f						; \
								; \
1:	cmp	dividend, divisor				; \
	subhs	dividend, dividend, divisor			; \
	cmp	dividend, divisor,  lsr #1			; \
	subhs	dividend, dividend, divisor, lsr #1		; \
	cmp	dividend, divisor,  lsr #2			; \
	subhs	dividend, dividend, divisor, lsr #2		; \
	cmp	dividend, divisor,  lsr #3			; \
	subhs	dividend, dividend, divisor, lsr #3		; \
	cmp	dividend, #1					; \
	mov	divisor, divisor, lsr #4			; \
	subges	order, order, #4				; \
	bge	1b						; \
								; \
	tst	order, #3					; \
	teqne	dividend, #0					; \
	beq	5f						; \
								; \
	/* Either 1, 2 or 3 comparison/substractions are left.	*/; \
2:	cmn	order, #2					; \
	blt	4f						; \
	beq	3f						; \
	cmp	dividend, divisor 				; \
	subhs	dividend, dividend, divisor			; \
	mov	divisor,  divisor,  lsr #1 			; \
3:	cmp	dividend, divisor 				; \
	subhs	dividend, dividend, divisor			; \
	mov	divisor,  divisor,  lsr #1 			; \
4:	cmp	dividend, divisor 				; \
	subhs	dividend, dividend, divisor			; \
5:								; \

#endif /* __ARM_ARCH__ < 5 || defined (__OPTIMIZE_SIZE__) */
/* ------------------------------------------------------------------------ */
#define THUMB_DIV_MOD_BODY(modulo)				  \
	/* Load the constant 0x10000000 into our work register.	*/; \
	mov	work, #1					; \
	lsl	work, #28					; \
LSYM(Loop1):							; \
	/* Unless the divisor is very big, shift it up in multiples of	*/; \
	/* four bits, since this is the amount of unwinding in the main 	*/; \
	/* division loop.  Continue shifting until the divisor is 	*/; \
	/* larger than the dividend. 				*/; \
	cmp	divisor, work					; \
	bhs	LSYM(Lbignum)					; \
	cmp	divisor, dividend 				; \
	bhs	LSYM(Lbignum)					; \
	lsl	divisor, #4					; \
	lsl	curbit,  #4					; \
	b	LSYM(Loop1)					; \
LSYM(Lbignum):							; \
	/* Set work to 0x80000000				*/; \
	lsl	work, #3					; \
LSYM(Loop2):							; \
	/* For very big divisors, we must shift it a bit at a time, or	*/; \
	/* we will be in danger of overflowing. 			*/; \
	cmp	divisor, work					; \
	bhs	LSYM(Loop3)					; \
	cmp	divisor, dividend 				; \
	bhs	LSYM(Loop3)					; \
	lsl	divisor, #1					; \
	lsl	curbit,  #1					; \
	b	LSYM(Loop2)					; \
LSYM(Loop3):							; \
	/* Test for possible subtractions ... 			*/; \
  .if modulo							; \
	/* ... On the final pass, this may subtract too much from the dividend, */; \
	/* so keep track of which subtractions are done, we can fix them up */; \
	/* afterwards.						*/; \
	mov	overdone, #0					; \
	cmp	dividend, divisor 				; \
	blo	LSYM(Lover1)					; \
	sub	dividend, dividend, divisor 			; \
LSYM(Lover1):							; \
	lsr	work, divisor, #1 				; \
	cmp	dividend, work					; \
	blo	LSYM(Lover2)					; \
	sub	dividend, dividend, work 			; \
	mov	ip, curbit					; \
	mov	work, #1					; \
	ror	curbit, work					; \
	orr	overdone, curbit				; \
	mov	curbit, ip					; \
LSYM(Lover2):							; \
	lsr	work, divisor, #2 				; \
	cmp	dividend, work					; \
	blo	LSYM(Lover3)					; \
	sub	dividend, dividend, work 			; \
	mov	ip, curbit					; \
	mov	work, #2					; \
	ror	curbit, work					; \
	orr	overdone, curbit				; \
	mov	curbit, ip					; \
LSYM(Lover3):							; \
	lsr	work, divisor, #3 				; \
	cmp	dividend, work					; \
	blo	LSYM(Lover4)					; \
	sub	dividend, dividend, work 			; \
	mov	ip, curbit					; \
	mov	work, #3					; \
	ror	curbit, work					; \
	orr	overdone, curbit				; \
	mov	curbit, ip					; \
LSYM(Lover4):							; \
	mov	ip, curbit					; \
  .else								; \
	/* ... and note which bits are done in the result.  On the final pass, 	*/; \
	/* this may subtract too much from the dividend, but the result will be ok, */; \
	/* since the "bit" will have been shifted out at the bottom. */; \
	cmp	dividend, divisor 				; \
	blo	LSYM(Lover1)					; \
	sub	dividend, dividend, divisor 			; \
	orr	result, result, curbit 				; \
LSYM(Lover1):							; \
	lsr	work, divisor, #1 				; \
	cmp	dividend, work					; \
	blo	LSYM(Lover2)					; \
	sub	dividend, dividend, work 			; \
	lsr	work, curbit, #1				; \
	orr	result, work					; \
LSYM(Lover2):							; \
	lsr	work, divisor, #2 				; \
	cmp	dividend, work					; \
	blo	LSYM(Lover3)					; \
	sub	dividend, dividend, work 			; \
	lsr	work, curbit, #2				; \
	orr	result, work					; \
LSYM(Lover3):							; \
	lsr	work, divisor, #3 				; \
	cmp	dividend, work					; \
	blo	LSYM(Lover4)					; \
	sub	dividend, dividend, work 			; \
	lsr	work, curbit, #3				; \
	orr	result, work					; \
LSYM(Lover4):							; \
  .endif							; \
								; \
	cmp	dividend, #0			/* Early termination?	*/; \
	beq	LSYM(Lover5)						; \
	lsr	curbit,  #4			/* No, any more bits to do?*/; \
	beq	LSYM(Lover5)					; \
	lsr	divisor, #4					; \
	b	LSYM(Loop3)					; \
LSYM(Lover5):							; \
  .if modulo							; \
	/* Any subtractions that we should not have done will be recorded in */; \
	/* the top three bits of "overdone".  Exactly which were not needed */; \
	/* are governed by the position of the bit, stored in ip. 	*/; \
	mov	work, #0xe					; \
	lsl	work, #28					; \
	and	overdone, work					; \
	beq	LSYM(Lgot_result) 				; \
								; \
	/* If we terminated early, because dividend became zero, then the*/; \
	/* bit in ip will not be in the bottom nibble, and we should not	*/; \
	/* perform the additions below.  We must test for this though 	*/; \
	/* (rather relying upon the TSTs to prevent the additions) since	*/; \
	/* the bit in ip could be in the top two bits which might then match*/; \
	/* with one of the smaller RORs.				*/; \
	mov	curbit, ip					; \
	mov	work, #0x7					; \
	tst	curbit, work					; \
	beq	LSYM(Lgot_result) 				; \
								; \
	mov	curbit, ip					; \
	mov	work, #3					; \
	ror	curbit, work					; \
	tst	overdone, curbit				; \
	beq	LSYM(Lover6)					; \
	lsr	work, divisor, #3 				; \
	add	dividend, work					; \
LSYM(Lover6):							; \
	mov	curbit, ip					; \
	mov	work, #2					; \
	ror	curbit, work					; \
	tst	overdone, curbit				; \
	beq	LSYM(Lover7)					; \
	lsr	work, divisor, #2 				; \
	add	dividend, work					; \
LSYM(Lover7):							; \
	mov	curbit, ip					; \
	mov	work, #1					; \
	ror	curbit, work					; \
	tst	overdone, curbit				; \
	beq	LSYM(Lgot_result) 				; \
	lsr	work, divisor, #1 				; \
	add	dividend, work					; \
  .endif							; \
LSYM(Lgot_result):
/* APPLE LOCAL end ARM MACH assembler macros */
/* ------------------------------------------------------------------------ */
/*		Start of the Real Functions				    */
/* ------------------------------------------------------------------------ */
#ifdef L_udivsi3

	FUNC_START udivsi3
	/* APPLE LOCAL ARM MACH assembler */
	FUNC_ALIAS (aeabi_uidiv, udivsi3)

#ifdef __thumb__

	cmp	divisor, #0
	beq	LSYM(Ldiv0)
	mov	curbit, #1
	mov	result, #0
	
	push	{ work }
	cmp	dividend, divisor
	blo	LSYM(Lgot_result)

	THUMB_DIV_MOD_BODY(0)
	
	mov	r0, result
	pop	{ work }
	RET

#else /* ARM version.  */

	subs	r2, r1, #1
	RETc(eq)
	bcc	LSYM(Ldiv0)
	cmp	r0, r1
	/* APPLE LOCAL ARM MACH assembler */
	bls	L11
	tst	r1, r2
	/* APPLE LOCAL ARM MACH assembler */
	beq	L12
	
	/* APPLE LOCAL ARM MACH assembler */
	ARM_DIV_BODY(r0, r1, r2, r3)
	
	mov	r0, r2
	RET	

/* APPLE LOCAL ARM MACH assembler */
L11:	moveq	r0, #1
	movne	r0, #0
	RET

/* APPLE LOCAL ARM MACH assembler */
L12:	ARM_DIV2_ORDER(r1, r2)

	mov	r0, r0, lsr r2
	RET

#endif /* ARM version */

	DIV_FUNC_END udivsi3

FUNC_START aeabi_uidivmod
#ifdef __thumb__
	push	{r0, r1, lr}
	bl	SYM(__udivsi3)
	pop	{r1, r2, r3}
	mul	r2, r0
	sub	r1, r1, r2
	bx	r3
#else
	stmfd	sp!, { r0, r1, lr }
	bl	SYM(__udivsi3)
	ldmfd	sp!, { r1, r2, lr }
	mul	r3, r2, r0
	sub	r1, r1, r3
	RET
#endif
	FUNC_END aeabi_uidivmod
	
#endif /* L_udivsi3 */
/* ------------------------------------------------------------------------ */
#ifdef L_umodsi3

	FUNC_START umodsi3

#ifdef __thumb__

	cmp	divisor, #0
	beq	LSYM(Ldiv0)
	mov	curbit, #1
	cmp	dividend, divisor
	bhs	LSYM(Lover10)
	RET	

LSYM(Lover10):
	push	{ work }

	THUMB_DIV_MOD_BODY(1)
	
	pop	{ work }
	RET
	
#else  /* ARM version.  */
	
	subs	r2, r1, #1			@ compare divisor with 1
	bcc	LSYM(Ldiv0)
	cmpne	r0, r1				@ compare dividend with divisor
	moveq   r0, #0
	tsthi	r1, r2				@ see if divisor is power of 2
	andeq	r0, r0, r2
	RETc(ls)

	/* APPLE LOCAL ARM MACH assembler */
	ARM_MOD_BODY(r0, r1, r2, r3)
	
	RET	

#endif /* ARM version.  */
	
	DIV_FUNC_END umodsi3

#endif /* L_umodsi3 */
/* ------------------------------------------------------------------------ */
#ifdef L_divsi3

	FUNC_START divsi3	
	/* APPLE LOCAL ARM MACH assembler */
	FUNC_ALIAS (aeabi_idiv, divsi3)

#ifdef __thumb__
	cmp	divisor, #0
	beq	LSYM(Ldiv0)
	
	push	{ work }
	mov	work, dividend
	eor	work, divisor		@ Save the sign of the result.
	mov	ip, work
	mov	curbit, #1
	mov	result, #0
	cmp	divisor, #0
	bpl	LSYM(Lover10)
	neg	divisor, divisor	@ Loops below use unsigned.
LSYM(Lover10):
	cmp	dividend, #0
	bpl	LSYM(Lover11)
	neg	dividend, dividend
LSYM(Lover11):
	cmp	dividend, divisor
	blo	LSYM(Lgot_result)

	THUMB_DIV_MOD_BODY(0)
	
	mov	r0, result
	mov	work, ip
	cmp	work, #0
	bpl	LSYM(Lover12)
	neg	r0, r0
LSYM(Lover12):
	pop	{ work }
	RET

#else /* ARM version.  */
	
	cmp	r1, #0
	eor	ip, r0, r1			@ save the sign of the result.
	beq	LSYM(Ldiv0)
	rsbmi	r1, r1, #0			@ loops below use unsigned.
	subs	r2, r1, #1			@ division by 1 or -1 ?
	/* APPLE LOCAL ARM MACH assembler */
	beq	L10
	movs	r3, r0
	rsbmi	r3, r0, #0			@ positive dividend value
	cmp	r3, r1
	/* APPLE LOCAL ARM MACH assembler */
	bls	L11
	tst	r1, r2				@ divisor is power of 2 ?
	/* APPLE LOCAL ARM MACH assembler */
	beq	L12

	/* APPLE LOCAL ARM MACH assembler */
	ARM_DIV_BODY(r3, r1, r0, r2)
	
	cmp	ip, #0
	rsbmi	r0, r0, #0
	RET	

/* APPLE LOCAL ARM MACH assembler */
L10:	teq	ip, r0				@ same sign ?
	rsbmi	r0, r0, #0
	RET	

/* APPLE LOCAL ARM MACH assembler */
L11:	movlo	r0, #0
	moveq	r0, ip, asr #31
	orreq	r0, r0, #1
	RET

/* APPLE LOCAL ARM MACH assembler */
L12:	ARM_DIV2_ORDER(r1, r2)

	cmp	ip, #0
	mov	r0, r3, lsr r2
	rsbmi	r0, r0, #0
	RET

#endif /* ARM version */
	
	DIV_FUNC_END divsi3

FUNC_START aeabi_idivmod
#ifdef __thumb__
	push	{r0, r1, lr}
	bl	SYM(__divsi3)
	pop	{r1, r2, r3}
	mul	r2, r0
	sub	r1, r1, r2
	bx	r3
#else
	stmfd	sp!, { r0, r1, lr }
	bl	SYM(__divsi3)
	ldmfd	sp!, { r1, r2, lr }
	mul	r3, r2, r0
	sub	r1, r1, r3
	RET
#endif
	FUNC_END aeabi_idivmod
	
#endif /* L_divsi3 */
/* ------------------------------------------------------------------------ */
#ifdef L_modsi3

	FUNC_START modsi3

#ifdef __thumb__

	mov	curbit, #1
	cmp	divisor, #0
	beq	LSYM(Ldiv0)
	bpl	LSYM(Lover10)
	neg	divisor, divisor		@ Loops below use unsigned.
LSYM(Lover10):
	push	{ work }
	@ Need to save the sign of the dividend, unfortunately, we need
	@ work later on.  Must do this after saving the original value of
	@ the work register, because we will pop this value off first.
	push	{ dividend }
	cmp	dividend, #0
	bpl	LSYM(Lover11)
	neg	dividend, dividend
LSYM(Lover11):
	cmp	dividend, divisor
	blo	LSYM(Lgot_result)

	THUMB_DIV_MOD_BODY(1)
		
	pop	{ work }
	cmp	work, #0
	bpl	LSYM(Lover12)
	neg	dividend, dividend
LSYM(Lover12):
	pop	{ work }
	RET	

#else /* ARM version.  */
	
	cmp	r1, #0
	beq	LSYM(Ldiv0)
	rsbmi	r1, r1, #0			@ loops below use unsigned.
	movs	ip, r0				@ preserve sign of dividend
	rsbmi	r0, r0, #0			@ if negative make positive
	subs	r2, r1, #1			@ compare divisor with 1
	cmpne	r0, r1				@ compare dividend with divisor
	moveq	r0, #0
	tsthi	r1, r2				@ see if divisor is power of 2
	andeq	r0, r0, r2
	/* APPLE LOCAL ARM MACH assembler */
	bls	L10

	/* APPLE LOCAL ARM MACH assembler */
	ARM_MOD_BODY(r0, r1, r2, r3)

/* APPLE LOCAL ARM MACH assembler */
L10:	cmp	ip, #0
	rsbmi	r0, r0, #0
	RET	

#endif /* ARM version */
	
	DIV_FUNC_END modsi3

#endif /* L_modsi3 */
/* ------------------------------------------------------------------------ */
#ifdef L_dvmd_tls

	FUNC_START div0
	/* APPLE LOCAL begin ARM MACH assembler */
	FUNC_ALIAS(aeabi_idiv0,div0)
	FUNC_ALIAS(aeabi_ldiv0,div0)
	/* APPLE LOCAL end ARM MACH assembler */

	RET

	FUNC_END aeabi_ldiv0
	FUNC_END aeabi_idiv0
	FUNC_END div0
	
#endif /* L_divmodsi_tools */
/* ------------------------------------------------------------------------ */
#ifdef L_dvmd_lnx
@ GNU/Linux division-by zero handler.  Used in place of L_dvmd_tls

/* Constant taken from <asm/signal.h>.  */
#define SIGFPE	8

	.code	32
	FUNC_START div0

	stmfd	sp!, {r1, lr}
	mov	r0, #SIGFPE
	bl	SYM(raise) __PLT__
	/* APPLE LOCAL ARM MACH assembler */
	RETLDM1 (r1)

	FUNC_END div0
	
#endif /* L_dvmd_lnx */
/* ------------------------------------------------------------------------ */
/* Dword shift operations.  */
/* All the following Dword shift variants rely on the fact that
	shft xxx, Reg
   is in fact done as
	shft xxx, (Reg & 255)
   so for Reg value in (32...63) and (-1...-31) we will get zero (in the
   case of logical shifts) or the sign (for asr).  */

#ifdef __ARMEB__
#define al	r1
#define ah	r0
#else
#define al	r0
#define ah	r1
#endif

/* Prevent __aeabi double-word shifts from being produced on SymbianOS.  */
#ifndef __symbian__

#ifdef L_lshrdi3

	FUNC_START lshrdi3
	/* APPLE LOCAL ARM MACH assembler */
	FUNC_ALIAS (aeabi_llsr, lshrdi3)
	
#ifdef __thumb__
	lsr	al, r2
	mov	r3, ah
	lsr	ah, r2
	mov	ip, r3
	sub	r2, #32
	lsr	r3, r2
	orr	al, r3
	neg	r2, r2
	mov	r3, ip
	lsl	r3, r2
	orr	al, r3
	RET
#else
	subs	r3, r2, #32
	rsb	ip, r2, #32
	movmi	al, al, lsr r2
	movpl	al, ah, lsr r3
	orrmi	al, al, ah, lsl ip
	mov	ah, ah, lsr r2
	RET
#endif
	FUNC_END aeabi_llsr
	FUNC_END lshrdi3

#endif
	
#ifdef L_ashrdi3
	
	FUNC_START ashrdi3
	/* APPLE LOCAL ARM MACH assembler */
	FUNC_ALIAS (aeabi_lasr, ashrdi3)
	
#ifdef __thumb__
	lsr	al, r2
	mov	r3, ah
	asr	ah, r2
	sub	r2, #32
	@ If r2 is negative at this point the following step would OR
	@ the sign bit into all of AL.  That's not what we want...
	bmi	1f
	mov	ip, r3
	asr	r3, r2
	orr	al, r3
	mov	r3, ip
1:
	neg	r2, r2
	lsl	r3, r2
	orr	al, r3
	RET
#else
	subs	r3, r2, #32
	rsb	ip, r2, #32
	movmi	al, al, lsr r2
	movpl	al, ah, asr r3
	orrmi	al, al, ah, lsl ip
	mov	ah, ah, asr r2
	RET
#endif

	FUNC_END aeabi_lasr
	FUNC_END ashrdi3

#endif

#ifdef L_ashldi3

	FUNC_START ashldi3
	/* APPLE LOCAL ARM MACH assembler */
	FUNC_ALIAS (aeabi_llsl, ashldi3)
	
#ifdef __thumb__
	lsl	ah, r2
	mov	r3, al
	lsl	al, r2
	mov	ip, r3
	sub	r2, #32
	lsl	r3, r2
	orr	ah, r3
	neg	r2, r2
	mov	r3, ip
	lsr	r3, r2
	orr	ah, r3
	RET
#else
	subs	r3, r2, #32
	rsb	ip, r2, #32
	movmi	ah, ah, lsl r2
	movpl	ah, al, lsl r3
	orrmi	ah, ah, al, lsr ip
	mov	al, al, lsl r2
	RET
#endif
	FUNC_END aeabi_llsl
	FUNC_END ashldi3

#endif

/* APPLE LOCAL begin ARM 4790140 compact switch tables */
/* ----------------------------------------------------------------------- */
/* These aren't needed for Thumb2 since then we have actual instructions
   to do what these functions do. */
#ifndef __thumb2__

/* Thumb switch table implementation.  Arm code, although must be called 
   from Thumb (the low bit of LR is expected to be 1).
   Expects the call site to be followed by 1-byte count, then <count>
   1-byte unsigned half-offsets (low bit of real offset is always 0, so
   not stored), then the half-offset for the default case (not included
   in the count). */

#ifdef L_switchu8

	FUNC_START switchu8

	ldrb ip, [lr, #-1]
	cmp r0, ip
	ldrccb r0, [lr, r0]
	ldrcsb r0, [lr, ip]
	add ip, lr, r0, lsl #1
	bx ip

	FUNC_END switchu8
#endif

/* Same with signed half-offsets. */

#ifdef L_switch8

	FUNC_START switch8

	ldrb ip, [lr, #-1]
	cmp r0, ip
	ldrccsb r0, [lr, r0]
	ldrcssb r0, [lr, ip]
	add ip, lr, r0, lsl #1
	bx ip

	FUNC_END switch8
#endif

/* Same with 16-bit signed half-offsets.   (This one is not
   all that efficient, there's no reg+reg<<const mode for
   halfwords.) */

#ifdef L_switch16

	FUNC_START switch16

	ldrh ip, [lr, #-1]
	cmp r0, ip
	add r0, lr, r0, lsl #1
	ldrccsh r0, [r0, #1]
	add ip, lr, ip, lsl #1
	ldrcssh r0, [ip, #1]
	add ip, lr, r0, lsl #1
	bx ip

	FUNC_END switch16
#endif

/* Same with 32-bit signed offset (shifting off the low
   bit would not gain anything here).  */

#ifdef L_switch32

	FUNC_START switch32

	ldr ip, [lr, #-1]
	cmp r0, ip
	add r0, lr, r0, lsl #2
	ldrcc r0, [r0, #3]
	add ip, lr, ip, lsl #2
	ldrcs r0, [ip, #3]
	add ip, lr, r0
	bx ip

	FUNC_END switch32
#endif

/* APPLE LOCAL begin 6465387 exception handling interworking VFP save */
#if (__ARM_ARCH__ == 6)
#ifdef L_save_vfp_d8_d15_regs 
        ARM_FUNC_START save_vfp_d8_d15_regs
        vpush {d8-d15}
        RET
        FUNC_END save_vfp_d8_d15_regs
#endif

#ifdef L_restore_vfp_d8_d15__regs
        ARM_FUNC_START restore_vfp_d8_d15_regs
        vpop {d8-d15}
        RET
        FUNC_END restore_vfp_d8_d15_regs
#endif
#endif
/* APPLE LOCAL end 6465387 exception handling interworking VFP save */

#endif /* !defined (__thumb2__) */
/* APPLE LOCAL end ARM 4790140 compact switch tables */

#endif /* __symbian__ */

/* ------------------------------------------------------------------------ */
/* These next two sections are here despite the fact that they contain Thumb 
   assembler because their presence allows interworked code to be linked even
   when the GCC library is this one.  */
		
/* Do not build the interworking functions when the target architecture does 
   not support Thumb instructions.  (This can be a multilib option).  */
#if defined __ARM_ARCH_4T__ || defined __ARM_ARCH_5T__\
      || defined __ARM_ARCH_5TE__ || defined __ARM_ARCH_5TEJ__ \
      || __ARM_ARCH__ >= 6

#if defined L_call_via_rX

/* These labels & instructions are used by the Arm/Thumb interworking code. 
   The address of function to be called is loaded into a register and then 
   one of these labels is called via a BL instruction.  This puts the 
   return address into the link register with the bottom bit set, and the 
   code here switches to the correct mode before executing the function.  */
	
	.text
	.align 0
        .force_thumb

/* APPLE LOCAL begin ARM MACH assembler */
#define call_via(register)						  \
	THUMB_FUNC_START _call_via_##register				; \
									; \
	bx	register						; \
	nop								; \
									; \
	SIZE	(_call_via_##register)

	call_via(r0)
	call_via(r1)
	call_via(r2)
	call_via(r3)
	call_via(r4)
	call_via(r5)
	call_via(r6)
	call_via(r7)
	call_via(r8)
	call_via(r9)
	call_via(sl)
	call_via(fp)
	call_via(ip)
	call_via(sp)
	call_via(lr)
/* APPLE LOCAL end ARM MACH assembler macros */

#endif /* L_call_via_rX */

/* ALQAAHIRA LOCAL begin v7 support. Merge from mainline */
/* Don't bother with the old interworking routines for Thumb-2.  */
/* ??? Maybe only omit these on v7m.  */
#ifndef __thumb2__

/* ALQAAHIRA LOCAL end v7 support. Merge from mainline */
#if defined L_interwork_call_via_rX

/* These labels & instructions are used by the Arm/Thumb interworking code,
   when the target address is in an unknown instruction set.  The address 
   of function to be called is loaded into a register and then one of these
   labels is called via a BL instruction.  This puts the return address 
   into the link register with the bottom bit set, and the code here 
   switches to the correct mode before executing the function.  Unfortunately
   the target code cannot be relied upon to return via a BX instruction, so
   instead we have to store the resturn address on the stack and allow the
   called function to return here instead.  Upon return we recover the real
   return address and use a BX to get back to Thumb mode.

   There are three variations of this code.  The first,
   _interwork_call_via_rN(), will push the return address onto the
   stack and pop it in _arm_return().  It should only be used if all
   arguments are passed in registers.

   The second, _interwork_r7_call_via_rN(), instead stores the return
   address at [r7, #-4].  It is the caller's responsibility to ensure
   that this address is valid and contains no useful data.

   The third, _interwork_r11_call_via_rN(), works in the same way but
   uses r11 instead of r7.  It is useful if the caller does not really
   need a frame pointer.  */
	
	.text
	.align 0

	.code   32
	.globl _arm_return
LSYM(Lstart_arm_return):
	cfi_start	LSYM(Lstart_arm_return) LSYM(Lend_arm_return)
	cfi_push	0, 0xe, -0x8, 0x8
	nop	@ This nop is for the benefit of debuggers, so that
		@ backtraces will use the correct unwind information.
_arm_return:
	/* APPLE LOCAL ARM MACH assembler */
	RETLDM_unwind (LSYM(Lstart_arm_return))
	cfi_end	LSYM(Lend_arm_return)

	.globl _arm_return_r7
_arm_return_r7:
	ldr	lr, [r7, #-4]
	bx	lr

	.globl _arm_return_r11
_arm_return_r11:
	ldr	lr, [r11, #-4]
	bx	lr

.macro interwork_with_frame frame, register, name, return
	.code	16

	THUMB_FUNC_START \name

	bx	pc
	nop

	.code	32
	tst	\register, #1
	streq	lr, [\frame, #-4]
	adreq	lr, _arm_return_\frame
	bx	\register

	SIZE	(\name)
.endm

.macro interwork register
	.code	16

	THUMB_FUNC_START _interwork_call_via_\register

	bx	pc
	nop

	.code	32
	.globl LSYM(Lchange_\register)
LSYM(Lchange_\register):
	tst	\register, #1
	streq	lr, [sp, #-8]!
	adreq	lr, _arm_return
	bx	\register

	SIZE	(_interwork_call_via_\register)

	interwork_with_frame r7,\register,_interwork_r7_call_via_\register
	interwork_with_frame r11,\register,_interwork_r11_call_via_\register
.endm
	
	interwork r0
	interwork r1
	interwork r2
	interwork r3
	interwork r4
	interwork r5
	interwork r6
	interwork r7
	interwork r8
	interwork r9
	interwork sl
	interwork fp
	interwork ip
	interwork sp
	
	/* The LR case has to be handled a little differently...  */
	.code 16

	THUMB_FUNC_START _interwork_call_via_lr

	bx 	pc
	nop
	
	.code 32
	.globl .Lchange_lr
.Lchange_lr:
	tst	lr, #1
	stmeqdb	r13!, {lr, pc}
	mov	ip, lr
	adreq	lr, _arm_return
	bx	ip
	
	SIZE	(_interwork_call_via_lr)
	
#endif /* L_interwork_call_via_rX */
/* ALQAAHIRA LOCAL v7 support. Merge from mainline */
#endif /* !__thumb2__ */
#endif /* Arch supports thumb.  */

#ifndef __symbian__
#include "ieee754-df.S"
#include "ieee754-sf.S"
#include "bpabi.S"
#endif /* __symbian__ */
