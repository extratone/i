#ifndef __GDB_ARM_MACOSX_REGNUMS_H__
#define __GDB_ARM_MACOSX_REGNUMS_H__

/* We assume that one of the two following header files will already
   have been included prior to including this header file:
   #include "arm-tdep.h"      (for gdb)
   #include "arm-regnums.h"   (for gdbserver)
 */

#define ARM_MACOSX_FIRST_VFP_STABS_REGNUM 63
#define ARM_MACOSX_LAST_VFP_STABS_REGNUM  94
#define ARM_MACOSX_NUM_GP_REGS 16	    /* R0-R15   */
#define ARM_MACOSX_NUM_GPS_REGS 1	    /* CPSR     */
#define ARM_MACOSX_NUM_FP_REGS 8	    /* F0-F7    */
#define ARM_MACOSX_NUM_FPS_REGS 1
#define ARM_MACOSX_NUM_VFP_REGS 32	    /* S0-S31   */
#define ARM_MACOSX_NUM_VFPS_REGS 1	    /* FPSCR    */
#define ARM_MACOSX_NUM_VFPV3_REGS 16	    /* D16-D31  */
#define ARM_MACOSX_NUM_VFPV1_PSEUDO_REGS 16 /* D0-D15   */
#define ARM_MACOSX_NUM_VFPV3_PSEUDO_REGS 32 /* D0-D15 and Q0-Q15 */
#define ARM_MACOSX_NUM_REGS (ARM_MACOSX_NUM_GP_REGS \
                             + ARM_MACOSX_NUM_GPS_REGS \
			     + ARM_MACOSX_NUM_FP_REGS \
			     + ARM_MACOSX_NUM_FPS_REGS)

#define ARM_V6_MACOSX_NUM_REGS (ARM_MACOSX_NUM_REGS \
                             + ARM_MACOSX_NUM_VFP_REGS \
			     + ARM_MACOSX_NUM_VFPS_REGS)

#define ARM_V7_MACOSX_NUM_REGS (ARM_MACOSX_NUM_REGS \
                             + ARM_MACOSX_NUM_VFP_REGS \
			     + ARM_MACOSX_NUM_VFPS_REGS \
			     + ARM_MACOSX_NUM_VFPV3_REGS)


#define ARM_MACOSX_IS_GP_REGNUM(regno) (((regno) >= ARM_R0_REGNUM) \
    && ((regno) <= ARM_PC_REGNUM))
#define ARM_MACOSX_IS_GPS_REGNUM(regno) ((regno) == ARM_PS_REGNUM)
/* Any GP reg: r0-r15, cpsr.  */
#define ARM_MACOSX_IS_GP_RELATED_REGNUM(regno) (ARM_MACOSX_IS_GP_REGNUM(regno) \
    || ARM_MACOSX_IS_GPS_REGNUM(regno))
/* Any FP reg: f0-f7, fps.  */
#define ARM_MACOSX_IS_FP_RELATED_REGNUM(regno)  (((regno) >= ARM_F0_REGNUM) \
    && ((regno) <= ARM_FPS_REGNUM))
/* Any VFP reg: s0-s31, d0-d15, fpscr.  */
#define ARM_MACOSX_IS_VFP_RELATED_REGNUM(regno) (((regno) >= ARM_VFP_REGNUM_S0) \
    && ((regno) <= ARM_SIMD_PSEUDO_REGNUM_Q15))

#endif /* __GDB_ARM_MACOSX_REGNUMS_H__ */
