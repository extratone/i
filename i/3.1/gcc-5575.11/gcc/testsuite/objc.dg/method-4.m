/* Check if class references (generated for the NeXT runtime) are appropriately 
   folded.  This test is safe to run on all targets.  */
/* Author: Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-options "-fnext-runtime" } */
/* { dg-do compile } */
/* APPLE LOCAL begin objc2 */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* { dg-skip-if "" { arm*-*-darwin* } { "*" } { "" } } */
/* APPLE LOCAL end objc2 */

/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"

typedef Object ObjectTypedef1;
typedef ObjectTypedef1 ObjectTypedef2;
@compatibility_alias ObjectAlias1 ObjectTypedef2;
@compatibility_alias ObjectAlias2 ObjectAlias1;
typedef ObjectAlias2 ObjectTypedef3;

void foo(void) {
  id obj = [Object new];
  obj = [ObjectTypedef1 new];
  obj = [ObjectTypedef2 new];
  obj = [ObjectTypedef3 new];
  obj = [ObjectAlias1 new];
  obj = [ObjectAlias2 new];
}

/* { dg-final { scan-assembler "_OBJC_CLASS_REFERENCES_0" } } */
/* { dg-final { scan-assembler-not "_OBJC_CLASS_REFERENCES_1" } } */
