extern void *memcpy (void *, const void *, __SIZE_TYPE__);
typedef int word_type;
   
static void
copy_reg (unsigned int reg, frame_state *udata,	/* { dg-error "parse|syntax|expected" } */
	  frame_state *target_udata)	/* { dg-error "expected" } */
{  
  word_type *preg = get_reg_addr (reg, udata, 0);	/* { dg-error "undeclared|function|without a cast" } */
  word_type *ptreg = get_reg_addr (reg, target_udata, 0); /* { dg-error "undeclared|without a cast" } */
   
  memcpy (ptreg, preg, __builtin_dwarf_reg_size (reg));
}
