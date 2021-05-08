#ifndef __GDB_ARM_MACOSX_THREAD_STATUS_H__
#define __GDB_ARM_MACOSX_THREAD_STATUS_H__

#define GDB_ARM_THREAD_STATE 1
#define GDB_ARM_THREAD_FPSTATE 2 /* Equivalent to ARM_VFP_STATE */
#define GDB_ARM_THREAD_EXCEPTION_STATE 3
#define GDB_ARM_THREAD_DEBUG_STATE 4

/* This structure comes from /usr/include/mach/arm/_types.h */
#include <stdint.h>

struct gdb_arm_thread_state
{
  uint32_t        r[16];          /* General purpose register r0-r15 */
  uint32_t        cpsr;           /* Current program status register */
};

typedef struct gdb_arm_thread_state gdb_arm_thread_state_t;

#define GDB_ARM_THREAD_STATE_COUNT \
    (sizeof (struct gdb_arm_thread_state) / sizeof (uint32_t))

struct gdb_arm_thread_vfpv1_state
{
  uint32_t r[32]; // S0 - S31 (and D0 - D15)
  uint32_t fpscr;
};

struct gdb_arm_thread_vfpv3_state
{
  uint32_t s[32]; // S0 - S31 (and D0 - D15)
  uint64_t d[16]; // D16 - D31 (and Q8 - Q15)
  uint32_t fpscr;
};

typedef struct gdb_arm_thread_vfpv1_state gdb_arm_thread_vfpv1_state_t;
typedef struct gdb_arm_thread_vfpv3_state gdb_arm_thread_vfpv3_state_t;

#define GDB_ARM_THREAD_FPSTATE_VFPV1_COUNT \
    (sizeof (struct gdb_arm_thread_vfpv1_state) / sizeof (uint32_t))
#define GDB_ARM_THREAD_FPSTATE_VFPV3_COUNT \
    (sizeof (struct gdb_arm_thread_vfpv3_state) / sizeof (uint32_t))

struct gdb_arm_thread_debug_state
{
  uint32_t bvr[16];
  uint32_t bcr[16];
  uint32_t wvr[16];
  uint32_t wcr[16];
};

typedef struct gdb_arm_thread_debug_state gdb_arm_thread_debug_state_t;
#define GDB_ARM_THREAD_DEBUG_STATE_COUNT \
    (sizeof (struct gdb_arm_thread_debug_state) / sizeof (uint32_t))


#endif /* __GDB_ARM_MACOSX_THREAD_STATUS_H__ */

