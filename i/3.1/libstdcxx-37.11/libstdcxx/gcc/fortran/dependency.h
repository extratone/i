/* Header for dependency analysis
   Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Paul Brook

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.  */



bool gfc_ref_needs_temporary_p (gfc_ref *);
bool gfc_full_array_ref_p (gfc_ref *);
gfc_expr *gfc_get_noncopying_intrinsic_argument (gfc_expr *);
int gfc_check_fncall_dependency (gfc_expr *, sym_intent, gfc_symbol *,
				 gfc_actual_arglist *);
int gfc_check_dependency (gfc_expr *, gfc_expr *, bool);
int gfc_is_same_range (gfc_array_ref *, gfc_array_ref *, int, int);
int gfc_expr_is_one (gfc_expr *, int);

int gfc_dep_resolver(gfc_ref *, gfc_ref *);
int gfc_are_equivalenced_arrays (gfc_expr *, gfc_expr *);
