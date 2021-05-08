#ifndef __GDB_ARM_MACOSX_TDEP_H__
#define __GDB_ARM_MACOSX_TDEP_H__
#include "defs.h"
#include "arm-macosx-thread-status.h"
#include "arm-macosx-regnums.h"

#define INVALID_ADDRESS ((CORE_ADDR) (-1))

void arm_macosx_fetch_gp_registers (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_fetch_gp_registers_raw (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_store_gp_registers (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_store_gp_registers_raw (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_fetch_vfpv1_regs (gdb_arm_thread_vfpv1_state_t *);
void arm_macosx_fetch_vfpv3_regs (gdb_arm_thread_vfpv3_state_t *);
void arm_macosx_fetch_vfpv1_regs_raw (gdb_arm_thread_vfpv1_state_t *);
void arm_macosx_fetch_vfpv3_regs_raw (gdb_arm_thread_vfpv3_state_t *);
void arm_macosx_store_vfpv1_regs (gdb_arm_thread_vfpv1_state_t *);
void arm_macosx_store_vfpv3_regs (gdb_arm_thread_vfpv3_state_t *);
void arm_macosx_store_vfpv1_regs_raw (gdb_arm_thread_vfpv1_state_t *);
void arm_macosx_store_vfpv3_regs_raw (gdb_arm_thread_vfpv3_state_t *);
int arm_macosx_keep_going (CORE_ADDR stop_pc);
void *arm_macosx_save_thread_inferior_status ();
void arm_macosx_restore_thread_inferior_status (void *);
void arm_macosx_free_thread_inferior_status (void *);

#endif /* __GDB_ARM_MACOSX_TDEP_H__ */
