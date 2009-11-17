/* Copyright (C) 2001,2008,2009 Free Software Foundation, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */


/* This file is included in vm_engine.c */


/*
 * Basic operations
 */

VM_DEFINE_INSTRUCTION (0, nop, "nop", 0, 0, 0)
{
  NEXT;
}

VM_DEFINE_INSTRUCTION (1, halt, "halt", 0, 0, 0)
{
  vp->time += scm_c_get_internal_run_time () - start_time;
  HALT_HOOK ();
  nvalues = SCM_I_INUM (*sp--);
  NULLSTACK (1);
  if (nvalues == 1)
    POP (finish_args);
  else
    {
      POP_LIST (nvalues);
      POP (finish_args);
      SYNC_REGISTER ();
      finish_args = scm_values (finish_args);
    }
    
  {
#ifdef VM_ENABLE_STACK_NULLING
    SCM *old_sp = sp;
#endif

    /* Restore registers */
    sp = SCM_FRAME_LOWER_ADDRESS (fp) - 1;
    /* Setting the ip here doesn't actually affect control flow, as the calling
       code will restore its own registers, but it does help when walking the
       stack */
    ip = SCM_FRAME_RETURN_ADDRESS (fp);
    fp = SCM_FRAME_DYNAMIC_LINK (fp);
    NULLSTACK (old_sp - sp);
  }
  
  goto vm_done;
}

VM_DEFINE_INSTRUCTION (2, break, "break", 0, 0, 0)
{
  BREAK_HOOK ();
  NEXT;
}

VM_DEFINE_INSTRUCTION (3, drop, "drop", 0, 1, 0)
{
  DROP ();
  NEXT;
}

VM_DEFINE_INSTRUCTION (4, dup, "dup", 0, 0, 1)
{
  SCM x = *sp;
  PUSH (x);
  NEXT;
}


/*
 * Object creation
 */

VM_DEFINE_INSTRUCTION (5, void, "void", 0, 0, 1)
{
  PUSH (SCM_UNSPECIFIED);
  NEXT;
}

VM_DEFINE_INSTRUCTION (6, make_true, "make-true", 0, 0, 1)
{
  PUSH (SCM_BOOL_T);
  NEXT;
}

VM_DEFINE_INSTRUCTION (7, make_false, "make-false", 0, 0, 1)
{
  PUSH (SCM_BOOL_F);
  NEXT;
}

VM_DEFINE_INSTRUCTION (8, make_eol, "make-eol", 0, 0, 1)
{
  PUSH (SCM_EOL);
  NEXT;
}

VM_DEFINE_INSTRUCTION (9, make_int8, "make-int8", 1, 0, 1)
{
  PUSH (SCM_I_MAKINUM ((signed char) FETCH ()));
  NEXT;
}

VM_DEFINE_INSTRUCTION (10, make_int8_0, "make-int8:0", 0, 0, 1)
{
  PUSH (SCM_INUM0);
  NEXT;
}

VM_DEFINE_INSTRUCTION (11, make_int8_1, "make-int8:1", 0, 0, 1)
{
  PUSH (SCM_I_MAKINUM (1));
  NEXT;
}

VM_DEFINE_INSTRUCTION (12, make_int16, "make-int16", 2, 0, 1)
{
  int h = FETCH ();
  int l = FETCH ();
  PUSH (SCM_I_MAKINUM ((signed short) (h << 8) + l));
  NEXT;
}

VM_DEFINE_INSTRUCTION (13, make_int64, "make-int64", 8, 0, 1)
{
  scm_t_uint64 v = 0;
  v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  PUSH (scm_from_int64 ((scm_t_int64) v));
  NEXT;
}

VM_DEFINE_INSTRUCTION (14, make_uint64, "make-uint64", 8, 0, 1)
{
  scm_t_uint64 v = 0;
  v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  PUSH (scm_from_uint64 (v));
  NEXT;
}

VM_DEFINE_INSTRUCTION (15, make_char8, "make-char8", 1, 0, 1)
{
  scm_t_uint8 v = 0;
  v = FETCH ();

  PUSH (SCM_MAKE_CHAR (v));
  /* Don't simplify this to PUSH (SCM_MAKE_CHAR (FETCH ())).  The
     contents of SCM_MAKE_CHAR may be evaluated more than once,
     resulting in a double fetch.  */
  NEXT;
}

VM_DEFINE_INSTRUCTION (16, make_char32, "make-char32", 4, 0, 1)
{
  scm_t_wchar v = 0;
  v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  v <<= 8; v += FETCH ();
  PUSH (SCM_MAKE_CHAR (v));
  NEXT;
}



VM_DEFINE_INSTRUCTION (17, list, "list", 2, -1, 1)
{
  unsigned h = FETCH ();
  unsigned l = FETCH ();
  unsigned len = ((h << 8) + l);
  POP_LIST (len);
  NEXT;
}

VM_DEFINE_INSTRUCTION (18, vector, "vector", 2, -1, 1)
{
  unsigned h = FETCH ();
  unsigned l = FETCH ();
  unsigned len = ((h << 8) + l);
  SCM vect;
  
  SYNC_REGISTER ();
  sp++; sp -= len;
  CHECK_UNDERFLOW ();
  vect = scm_make_vector (scm_from_uint (len), SCM_BOOL_F);
  memcpy (SCM_I_VECTOR_WELTS(vect), sp, sizeof(SCM) * len);
  NULLSTACK (len);
  *sp = vect;

  NEXT;
}


/*
 * Variable access
 */

#define OBJECT_REF(i)		objects[i]
#define OBJECT_SET(i,o)		objects[i] = o

#define LOCAL_REF(i)		SCM_FRAME_VARIABLE (fp, i)
#define LOCAL_SET(i,o)		SCM_FRAME_VARIABLE (fp, i) = o

/* For the variable operations, we _must_ obviously avoid function calls to
   `scm_variable_ref ()', `scm_variable_bound_p ()' and friends which do
   nothing more than the corresponding macros.  */
#define VARIABLE_REF(v)		SCM_VARIABLE_REF (v)
#define VARIABLE_SET(v,o)	SCM_VARIABLE_SET (v, o)
#define VARIABLE_BOUNDP(v)      (VARIABLE_REF (v) != SCM_UNDEFINED)

#define FREE_VARIABLE_REF(i)	free_vars[i]

/* ref */

VM_DEFINE_INSTRUCTION (19, object_ref, "object-ref", 1, 0, 1)
{
  register unsigned objnum = FETCH ();
  CHECK_OBJECT (objnum);
  PUSH (OBJECT_REF (objnum));
  NEXT;
}

/* FIXME: necessary? elt 255 of the vector could be a vector... */
VM_DEFINE_INSTRUCTION (20, long_object_ref, "long-object-ref", 2, 0, 1)
{
  unsigned int objnum = FETCH ();
  objnum <<= 8;
  objnum += FETCH ();
  CHECK_OBJECT (objnum);
  PUSH (OBJECT_REF (objnum));
  NEXT;
}

VM_DEFINE_INSTRUCTION (21, local_ref, "local-ref", 1, 0, 1)
{
  PUSH (LOCAL_REF (FETCH ()));
  ASSERT_BOUND (*sp);
  NEXT;
}

VM_DEFINE_INSTRUCTION (22, long_local_ref, "long-local-ref", 2, 0, 1)
{
  unsigned int i = FETCH ();
  i <<= 8;
  i += FETCH ();
  PUSH (LOCAL_REF (i));
  ASSERT_BOUND (*sp);
  NEXT;
}

VM_DEFINE_INSTRUCTION (23, local_bound, "local-bound?", 1, 0, 1)
{
  if (LOCAL_REF (FETCH ()) == SCM_UNDEFINED)
    PUSH (SCM_BOOL_F);
  else
    PUSH (SCM_BOOL_T);
  NEXT;
}

VM_DEFINE_INSTRUCTION (24, long_local_bound, "long-local-bound?", 2, 0, 1)
{
  unsigned int i = FETCH ();
  i <<= 8;
  i += FETCH ();
  if (LOCAL_REF (i) == SCM_UNDEFINED)
    PUSH (SCM_BOOL_F);
  else
    PUSH (SCM_BOOL_T);
  NEXT;
}

VM_DEFINE_INSTRUCTION (25, variable_ref, "variable-ref", 0, 0, 1)
{
  SCM x = *sp;

  if (!VARIABLE_BOUNDP (x))
    {
      finish_args = scm_list_1 (x);
      /* Was: finish_args = SCM_LIST1 (SCM_CAR (x)); */
      goto vm_error_unbound;
    }
  else
    {
      SCM o = VARIABLE_REF (x);
      *sp = o;
    }

  NEXT;
}

VM_DEFINE_INSTRUCTION (26, variable_bound, "variable-bound?", 0, 0, 1)
{
  if (VARIABLE_BOUNDP (*sp))
    *sp = SCM_BOOL_T;
  else
    *sp = SCM_BOOL_F;
  NEXT;
}

VM_DEFINE_INSTRUCTION (27, toplevel_ref, "toplevel-ref", 1, 0, 1)
{
  unsigned objnum = FETCH ();
  SCM what;
  CHECK_OBJECT (objnum);
  what = OBJECT_REF (objnum);

  if (!SCM_VARIABLEP (what)) 
    {
      SYNC_REGISTER ();
      what = resolve_variable (what, scm_program_module (program));
      if (!VARIABLE_BOUNDP (what))
        {
          finish_args = scm_list_1 (what);
          goto vm_error_unbound;
        }
      OBJECT_SET (objnum, what);
    }

  PUSH (VARIABLE_REF (what));
  NEXT;
}

VM_DEFINE_INSTRUCTION (28, long_toplevel_ref, "long-toplevel-ref", 2, 0, 1)
{
  SCM what;
  unsigned int objnum = FETCH ();
  objnum <<= 8;
  objnum += FETCH ();
  CHECK_OBJECT (objnum);
  what = OBJECT_REF (objnum);

  if (!SCM_VARIABLEP (what)) 
    {
      SYNC_REGISTER ();
      what = resolve_variable (what, scm_program_module (program));
      if (!VARIABLE_BOUNDP (what))
        {
          finish_args = scm_list_1 (what);
          goto vm_error_unbound;
        }
      OBJECT_SET (objnum, what);
    }

  PUSH (VARIABLE_REF (what));
  NEXT;
}

/* set */

VM_DEFINE_INSTRUCTION (29, local_set, "local-set", 1, 1, 0)
{
  LOCAL_SET (FETCH (), *sp);
  DROP ();
  NEXT;
}

VM_DEFINE_INSTRUCTION (30, long_local_set, "long-local-set", 2, 1, 0)
{
  unsigned int i = FETCH ();
  i <<= 8;
  i += FETCH ();
  LOCAL_SET (i, *sp);
  DROP ();
  NEXT;
}

VM_DEFINE_INSTRUCTION (31, variable_set, "variable-set", 0, 1, 0)
{
  VARIABLE_SET (sp[0], sp[-1]);
  DROPN (2);
  NEXT;
}

VM_DEFINE_INSTRUCTION (32, toplevel_set, "toplevel-set", 1, 1, 0)
{
  unsigned objnum = FETCH ();
  SCM what;
  CHECK_OBJECT (objnum);
  what = OBJECT_REF (objnum);

  if (!SCM_VARIABLEP (what)) 
    {
      SYNC_BEFORE_GC ();
      what = resolve_variable (what, scm_program_module (program));
      OBJECT_SET (objnum, what);
    }

  VARIABLE_SET (what, *sp);
  DROP ();
  NEXT;
}

VM_DEFINE_INSTRUCTION (33, long_toplevel_set, "long-toplevel-set", 2, 1, 0)
{
  SCM what;
  unsigned int objnum = FETCH ();
  objnum <<= 8;
  objnum += FETCH ();
  CHECK_OBJECT (objnum);
  what = OBJECT_REF (objnum);

  if (!SCM_VARIABLEP (what)) 
    {
      SYNC_BEFORE_GC ();
      what = resolve_variable (what, scm_program_module (program));
      OBJECT_SET (objnum, what);
    }

  VARIABLE_SET (what, *sp);
  DROP ();
  NEXT;
}


/*
 * branch and jump
 */

/* offset must be at least 24 bits wide, and signed */
#define FETCH_OFFSET(offset)                    \
{						\
  offset = FETCH () << 16;                      \
  offset += FETCH () << 8;                      \
  offset += FETCH ();                           \
  offset -= (offset & (1<<23)) << 1;            \
}

#define BR(p)					\
{						\
  scm_t_int32 offset;                           \
  FETCH_OFFSET (offset);                        \
  if (p)					\
    ip += offset;                               \
  NULLSTACK (1);				\
  DROP ();					\
  NEXT;						\
}

VM_DEFINE_INSTRUCTION (34, br, "br", 3, 0, 0)
{
  scm_t_int32 offset;
  FETCH_OFFSET (offset);
  ip += offset;
  NEXT;
}

VM_DEFINE_INSTRUCTION (35, br_if, "br-if", 3, 0, 0)
{
  BR (scm_is_true_and_not_nil (*sp));
}

VM_DEFINE_INSTRUCTION (36, br_if_not, "br-if-not", 3, 0, 0)
{
  BR (scm_is_false_or_nil (*sp));
}

VM_DEFINE_INSTRUCTION (37, br_if_eq, "br-if-eq", 3, 0, 0)
{
  sp--; /* underflow? */
  BR (SCM_EQ_P (sp[0], sp[1]));
}

VM_DEFINE_INSTRUCTION (38, br_if_not_eq, "br-if-not-eq", 3, 0, 0)
{
  sp--; /* underflow? */
  BR (!SCM_EQ_P (sp[0], sp[1]));
}

VM_DEFINE_INSTRUCTION (39, br_if_null, "br-if-null", 3, 0, 0)
{
  BR (scm_is_null_or_nil (*sp));
}

VM_DEFINE_INSTRUCTION (40, br_if_not_null, "br-if-not-null", 3, 0, 0)
{
  BR (!scm_is_null_or_nil (*sp));
}


/*
 * Subprogram call
 */

VM_DEFINE_INSTRUCTION (41, br_if_nargs_ne, "br-if-nargs-ne", 5, 0, 0)
{
  scm_t_ptrdiff n;
  scm_t_int32 offset;
  n = FETCH () << 8;
  n += FETCH ();
  FETCH_OFFSET (offset);
  if (sp - (fp - 1) != n)
    ip += offset;
  NEXT;
}

VM_DEFINE_INSTRUCTION (42, br_if_nargs_lt, "br-if-nargs-lt", 5, 0, 0)
{
  scm_t_ptrdiff n;
  scm_t_int32 offset;
  n = FETCH () << 8;
  n += FETCH ();
  FETCH_OFFSET (offset);
  if (sp - (fp - 1) < n)
    ip += offset;
  NEXT;
}

VM_DEFINE_INSTRUCTION (43, br_if_nargs_gt, "br-if-nargs-gt", 5, 0, 0)
{
  scm_t_ptrdiff n;
  scm_t_int32 offset;

  n = FETCH () << 8;
  n += FETCH ();
  FETCH_OFFSET (offset);
  if (sp - (fp - 1) > n)
    ip += offset;
  NEXT;
}

VM_DEFINE_INSTRUCTION (44, assert_nargs_ee, "assert-nargs-ee", 2, 0, 0)
{
  scm_t_ptrdiff n;
  n = FETCH () << 8;
  n += FETCH ();
  if (sp - (fp - 1) != n)
    goto vm_error_wrong_num_args;
  NEXT;
}

VM_DEFINE_INSTRUCTION (45, assert_nargs_ge, "assert-nargs-ge", 2, 0, 0)
{
  scm_t_ptrdiff n;
  n = FETCH () << 8;
  n += FETCH ();
  if (sp - (fp - 1) < n)
    goto vm_error_wrong_num_args;
  NEXT;
}

VM_DEFINE_INSTRUCTION (46, bind_optionals, "bind-optionals", 2, -1, -1)
{
  scm_t_ptrdiff n;
  n = FETCH () << 8;
  n += FETCH ();
  while (sp - (fp - 1) < n)
    PUSH (SCM_UNDEFINED);
  NEXT;
}

VM_DEFINE_INSTRUCTION (47, bind_optionals_shuffle, "bind-optionals/shuffle", 6, -1, -1)
{
  SCM *walk;
  scm_t_ptrdiff nreq, nreq_and_opt, ntotal;
  nreq = FETCH () << 8;
  nreq += FETCH ();
  nreq_and_opt = FETCH () << 8;
  nreq_and_opt += FETCH ();
  ntotal = FETCH () << 8;
  ntotal += FETCH ();

  /* look in optionals for first keyword or last positional */
  /* starting after the last required positional arg */
  walk = fp + nreq;
  while (/* while we have args */
         walk <= sp
         /* and we still have positionals to fill */
         && walk - fp < nreq_and_opt
         /* and we haven't reached a keyword yet */
         && !scm_is_keyword (*walk))
    /* bind this optional arg (by leaving it in place) */
    walk++;
  /* now shuffle up, from walk to ntotal */
  {
    scm_t_ptrdiff nshuf = sp - walk + 1, i;
    sp = (fp - 1) + ntotal + nshuf;
    CHECK_OVERFLOW ();
    for (i = 0; i < nshuf; i++)
      sp[-i] = walk[nshuf-i-1];
  }
  /* and fill optionals & keyword args with SCM_UNDEFINED */
  while (walk <= (fp - 1) + ntotal)
    *walk++ = SCM_UNDEFINED;

  NEXT;
}

/* Flags that determine whether other keywords are allowed, and whether a
   rest argument is expected.  These values must match those used by the
   glil->assembly compiler.  */
#define F_ALLOW_OTHER_KEYS  1
#define F_REST              2

VM_DEFINE_INSTRUCTION (48, bind_kwargs, "bind-kwargs", 5, 0, 0)
{
  scm_t_uint16 idx;
  scm_t_ptrdiff nkw;
  int kw_and_rest_flags;
  SCM kw;
  idx = FETCH () << 8;
  idx += FETCH ();
  /* XXX: We don't actually use NKW.  */
  nkw = FETCH () << 8;
  nkw += FETCH ();
  kw_and_rest_flags = FETCH ();

  if (!(kw_and_rest_flags & F_REST)
      && ((sp - (fp - 1) - nkw) % 2))
    goto vm_error_kwargs_length_not_even;

  CHECK_OBJECT (idx);
  kw = OBJECT_REF (idx);

  /* Switch NKW to be a negative index below SP.  */
  for (nkw = -(sp - (fp - 1) - nkw) + 1; nkw < 0; nkw++)
    {
      SCM walk;

      if (scm_is_keyword (sp[nkw]))
	{
	  for (walk = kw; scm_is_pair (walk); walk = SCM_CDR (walk))
	    {
	      if (scm_is_eq (SCM_CAAR (walk), sp[nkw]))
		{
		  SCM si = SCM_CDAR (walk);
		  LOCAL_SET (SCM_I_INUMP (si) ? SCM_I_INUM (si) : scm_to_long (si),
			     sp[nkw + 1]);
		  break;
		}
	    }
	  if (!(kw_and_rest_flags & F_ALLOW_OTHER_KEYS) && !scm_is_pair (walk))
	    goto vm_error_kwargs_unrecognized_keyword;

	  nkw++;
	}
      else if (!(kw_and_rest_flags & F_REST))
        goto vm_error_kwargs_invalid_keyword;
    }

  NEXT;
}

#undef F_ALLOW_OTHER_KEYS
#undef F_REST


VM_DEFINE_INSTRUCTION (49, push_rest, "push-rest", 2, -1, -1)
{
  scm_t_ptrdiff n;
  SCM rest = SCM_EOL;
  n = FETCH () << 8;
  n += FETCH ();
  while (sp - (fp - 1) > n)
    /* No need to check for underflow. */
    CONS (rest, *sp--, rest);
  PUSH (rest);
  NEXT;
}

VM_DEFINE_INSTRUCTION (50, bind_rest, "bind-rest", 4, -1, -1)
{
  scm_t_ptrdiff n;
  scm_t_uint32 i;
  SCM rest = SCM_EOL;
  n = FETCH () << 8;
  n += FETCH ();
  i = FETCH () << 8;
  i += FETCH ();
  while (sp - (fp - 1) > n)
    /* No need to check for underflow. */
    CONS (rest, *sp--, rest);
  LOCAL_SET (i, rest);
  NEXT;
}

VM_DEFINE_INSTRUCTION (51, reserve_locals, "reserve-locals", 2, -1, -1)
{
  SCM *old_sp;
  scm_t_int32 n;
  n = FETCH () << 8;
  n += FETCH ();
  old_sp = sp;
  sp = (fp - 1) + n;

  if (old_sp < sp)
    {
      CHECK_OVERFLOW ();
      while (old_sp < sp)
        *++old_sp = SCM_UNDEFINED;
    }
  else
    NULLSTACK (old_sp - sp);

  NEXT;
}

VM_DEFINE_INSTRUCTION (52, new_frame, "new-frame", 0, 0, 3)
{
  /* NB: if you change this, see frames.c:vm-frame-num-locals */
  /* and frames.h, vm-engine.c, etc of course */
  PUSH ((SCM)fp); /* dynamic link */
  PUSH (0);  /* mvra */
  PUSH (0);  /* ra */
  NEXT;
}

VM_DEFINE_INSTRUCTION (53, call, "call", 1, -1, 1)
{
  SCM x;
  nargs = FETCH ();

 vm_call:
  x = sp[-nargs];

  SYNC_REGISTER ();
  SCM_TICK;	/* allow interrupt here */

  /*
   * Subprogram call
   */
  if (SCM_PROGRAM_P (x))
    {
      program = x;
      CACHE_PROGRAM ();
      fp = sp - nargs + 1;
      ASSERT (SCM_FRAME_RETURN_ADDRESS (fp) == 0);
      ASSERT (SCM_FRAME_MV_RETURN_ADDRESS (fp) == 0);
      SCM_FRAME_SET_RETURN_ADDRESS (fp, ip);
      SCM_FRAME_SET_MV_RETURN_ADDRESS (fp, 0);
      ip = bp->base;
      ENTER_HOOK ();
      APPLY_HOOK ();
      NEXT;
    }
  if (SCM_STRUCTP (x) && SCM_OBJ_CLASS_FLAGS (x) & SCM_CLASSF_PURE_GENERIC)
    {
      SCM args = SCM_EOL;
      int n = nargs;
      SCM* walk = sp;
      SYNC_REGISTER ();
      while (n--)
        args = scm_cons (*walk--, args);
      *walk = scm_mcache_compute_cmethod (SCM_GENERIC_METHOD_CACHE (x), args);
      goto vm_call;
    }
  /*
   * Other interpreted or compiled call
   */
  if (!SCM_FALSEP (scm_procedure_p (x)))
    {
      SCM args;
      /* At this point, the stack contains the frame, the procedure and each one
	 of its arguments. */
      POP_LIST (nargs);
      POP (args);
      DROP (); /* drop the procedure */
      DROP_FRAME ();
      
      SYNC_REGISTER ();
      PUSH (scm_apply (x, args, SCM_EOL));
      NULLSTACK_FOR_NONLOCAL_EXIT ();
      if (SCM_UNLIKELY (SCM_VALUESP (*sp)))
        {
          /* truncate values */
          SCM values;
          POP (values);
          values = scm_struct_ref (values, SCM_INUM0);
          if (scm_is_null (values))
            goto vm_error_not_enough_values;
          PUSH (SCM_CAR (values));
        }
      NEXT;
    }

  program = x;
  goto vm_error_wrong_type_apply;
}

VM_DEFINE_INSTRUCTION (54, goto_args, "goto/args", 1, -1, 1)
{
  register SCM x;
  nargs = FETCH ();
 vm_goto_args:
  x = sp[-nargs];

  SYNC_REGISTER ();
  SCM_TICK;	/* allow interrupt here */

  /*
   * Tail call
   */
  if (SCM_PROGRAM_P (x))
    {
      int i;
#ifdef VM_ENABLE_STACK_NULLING
      SCM *old_sp = sp;
      CHECK_STACK_LEAK ();
#endif

      EXIT_HOOK ();

      /* switch programs */
      program = x;
      CACHE_PROGRAM ();
      /* shuffle down the program and the arguments */
      for (i = -1, sp = sp - nargs + 1; i < nargs; i++)
        SCM_FRAME_STACK_ADDRESS (fp)[i] = sp[i];

      sp = fp + i - 1;

      NULLSTACK (old_sp - sp);

      ip = bp->base;

      ENTER_HOOK ();
      APPLY_HOOK ();
      NEXT;
    }
  if (SCM_STRUCTP (x) && SCM_OBJ_CLASS_FLAGS (x) & SCM_CLASSF_PURE_GENERIC)
    {
      SCM args = SCM_EOL;
      int n = nargs;
      SCM* walk = sp;
      SYNC_REGISTER ();
      while (n--)
        args = scm_cons (*walk--, args);
      *walk = scm_mcache_compute_cmethod (SCM_GENERIC_METHOD_CACHE (x), args);
      goto vm_goto_args;
    }

  /*
   * Other interpreted or compiled call
   */
  if (!SCM_FALSEP (scm_procedure_p (x)))
    {
      SCM args;
      POP_LIST (nargs);
      POP (args);

      SYNC_REGISTER ();
      *sp = scm_apply (x, args, SCM_EOL);
      NULLSTACK_FOR_NONLOCAL_EXIT ();

      if (SCM_UNLIKELY (SCM_VALUESP (*sp)))
        {
          /* multiple values returned to continuation */
          SCM values;
          POP (values);
          values = scm_struct_ref (values, SCM_INUM0);
          nvalues = scm_ilength (values);
          PUSH_LIST (values, SCM_NULLP);
          goto vm_return_values;
        }
      else
        goto vm_return;
    }

  program = x;

  goto vm_error_wrong_type_apply;
}

VM_DEFINE_INSTRUCTION (55, goto_nargs, "goto/nargs", 0, 0, 1)
{
  SCM x;
  POP (x);
  nargs = scm_to_int (x);
  /* FIXME: should truncate values? */
  goto vm_goto_args;
}

VM_DEFINE_INSTRUCTION (56, call_nargs, "call/nargs", 0, 0, 1)
{
  SCM x;
  POP (x);
  nargs = scm_to_int (x);
  /* FIXME: should truncate values? */
  goto vm_call;
}

VM_DEFINE_INSTRUCTION (57, mv_call, "mv-call", 4, -1, 1)
{
  SCM x;
  scm_t_int32 offset;
  scm_t_uint8 *mvra;
  
  nargs = FETCH ();
  FETCH_OFFSET (offset);
  mvra = ip + offset;

 vm_mv_call:
  x = sp[-nargs];

  /*
   * Subprogram call
   */
  if (SCM_PROGRAM_P (x))
    {
      program = x;
      CACHE_PROGRAM ();
      fp = sp - nargs + 1;
      ASSERT (SCM_FRAME_RETURN_ADDRESS (fp) == 0);
      ASSERT (SCM_FRAME_MV_RETURN_ADDRESS (fp) == 0);
      SCM_FRAME_SET_RETURN_ADDRESS (fp, ip);
      SCM_FRAME_SET_MV_RETURN_ADDRESS (fp, mvra);
      ip = bp->base;
      ENTER_HOOK ();
      APPLY_HOOK ();
      NEXT;
    }
  if (SCM_STRUCTP (x) && SCM_OBJ_CLASS_FLAGS (x) & SCM_CLASSF_PURE_GENERIC)
    {
      SCM args = SCM_EOL;
      int n = nargs;
      SCM* walk = sp;
      SYNC_REGISTER ();
      while (n--)
        args = scm_cons (*walk--, args);
      *walk = scm_mcache_compute_cmethod (SCM_GENERIC_METHOD_CACHE (x), args);
      goto vm_mv_call;
    }
  /*
   * Other interpreted or compiled call
   */
  if (!SCM_FALSEP (scm_procedure_p (x)))
    {
      SCM args;
      /* At this point, the stack contains the procedure and each one of its
	 arguments.  */
      POP_LIST (nargs);
      POP (args);
      DROP (); /* drop the procedure */
      DROP_FRAME ();
      
      SYNC_REGISTER ();
      PUSH (scm_apply (x, args, SCM_EOL));
      NULLSTACK_FOR_NONLOCAL_EXIT ();
      if (SCM_VALUESP (*sp))
        {
          SCM values, len;
          POP (values);
          values = scm_struct_ref (values, SCM_INUM0);
          len = scm_length (values);
          PUSH_LIST (values, SCM_NULLP);
          PUSH (len);
          ip = mvra;
        }
      NEXT;
    }

  program = x;
  goto vm_error_wrong_type_apply;
}

VM_DEFINE_INSTRUCTION (58, apply, "apply", 1, -1, 1)
{
  int len;
  SCM ls;
  POP (ls);

  nargs = FETCH ();
  ASSERT (nargs >= 2);

  len = scm_ilength (ls);
  if (len < 0)
    goto vm_error_wrong_type_arg;

  PUSH_LIST (ls, SCM_NULL_OR_NIL_P);

  nargs += len - 2;
  goto vm_call;
}

VM_DEFINE_INSTRUCTION (59, goto_apply, "goto/apply", 1, -1, 1)
{
  int len;
  SCM ls;
  POP (ls);

  nargs = FETCH ();
  ASSERT (nargs >= 2);

  len = scm_ilength (ls);
  if (len < 0)
    goto vm_error_wrong_type_arg;

  PUSH_LIST (ls, SCM_NULL_OR_NIL_P);

  nargs += len - 2;
  goto vm_goto_args;
}

VM_DEFINE_INSTRUCTION (60, call_cc, "call/cc", 0, 1, 1)
{
  int first;
  SCM proc, cont;
  POP (proc);
  SYNC_ALL ();
  cont = scm_make_continuation (&first);
  if (first) 
    {
      PUSH ((SCM)fp); /* dynamic link */
      PUSH (0);  /* mvra */
      PUSH (0);  /* ra */
      PUSH (proc);
      PUSH (cont);
      nargs = 1;
      goto vm_call;
    }
  ASSERT (sp == vp->sp);
  ASSERT (fp == vp->fp);
  else if (SCM_VALUESP (cont))
    {
      /* multiple values returned to continuation */
      SCM values;
      values = scm_struct_ref (cont, SCM_INUM0);
      if (SCM_NULLP (values))
        goto vm_error_no_values;
      /* non-tail context does not accept multiple values? */
      PUSH (SCM_CAR (values));
      NEXT;
    }
  else
    {
      PUSH (cont);
      NEXT;
    }
}

VM_DEFINE_INSTRUCTION (61, goto_cc, "goto/cc", 0, 1, 1)
{
  int first;
  SCM proc, cont;
  POP (proc);
  SYNC_ALL ();
  cont = scm_make_continuation (&first);
  ASSERT (sp == vp->sp);
  ASSERT (fp == vp->fp);
  if (first) 
    {
      PUSH (proc);
      PUSH (cont);
      nargs = 1;
      goto vm_goto_args;
    }
  else if (SCM_VALUESP (cont))
    {
      /* multiple values returned to continuation */
      SCM values;
      values = scm_struct_ref (cont, SCM_INUM0);
      nvalues = scm_ilength (values);
      PUSH_LIST (values, SCM_NULLP);
      goto vm_return_values;
    }
  else
    {
      PUSH (cont);
      goto vm_return;
    }
}

VM_DEFINE_INSTRUCTION (62, return, "return", 0, 1, 1)
{
 vm_return:
  EXIT_HOOK ();
  RETURN_HOOK ();
  SYNC_REGISTER ();
  SCM_TICK;	/* allow interrupt here */
  {
    SCM ret;

    POP (ret);

#ifdef VM_ENABLE_STACK_NULLING
    SCM *old_sp = sp;
#endif

    /* Restore registers */
    sp = SCM_FRAME_LOWER_ADDRESS (fp);
    ip = SCM_FRAME_RETURN_ADDRESS (fp);
    fp = SCM_FRAME_DYNAMIC_LINK (fp);

#ifdef VM_ENABLE_STACK_NULLING
    NULLSTACK (old_sp - sp);
#endif

    /* Set return value (sp is already pushed) */
    *sp = ret;
  }

  /* Restore the last program */
  program = SCM_FRAME_PROGRAM (fp);
  CACHE_PROGRAM ();
  CHECK_IP ();
  NEXT;
}

VM_DEFINE_INSTRUCTION (63, return_values, "return/values", 1, -1, -1)
{
  /* nvalues declared at top level, because for some reason gcc seems to think
     that perhaps it might be used without declaration. Fooey to that, I say. */
  nvalues = FETCH ();
 vm_return_values:
  EXIT_HOOK ();
  RETURN_HOOK ();

  if (nvalues != 1 && SCM_FRAME_MV_RETURN_ADDRESS (fp)) 
    {
      /* A multiply-valued continuation */
      SCM *vals = sp - nvalues;
      int i;
      /* Restore registers */
      sp = SCM_FRAME_LOWER_ADDRESS (fp) - 1;
      ip = SCM_FRAME_MV_RETURN_ADDRESS (fp);
      fp = SCM_FRAME_DYNAMIC_LINK (fp);
        
      /* Push return values, and the number of values */
      for (i = 0; i < nvalues; i++)
        *++sp = vals[i+1];
      *++sp = SCM_I_MAKINUM (nvalues);
             
      /* Finally null the end of the stack */
      NULLSTACK (vals + nvalues - sp);
    }
  else if (nvalues >= 1)
    {
      /* Multiple values for a single-valued continuation -- here's where I
         break with guile tradition and try and do something sensible. (Also,
         this block handles the single-valued return to an mv
         continuation.) */
      SCM *vals = sp - nvalues;
      /* Restore registers */
      sp = SCM_FRAME_LOWER_ADDRESS (fp) - 1;
      ip = SCM_FRAME_RETURN_ADDRESS (fp);
      fp = SCM_FRAME_DYNAMIC_LINK (fp);
        
      /* Push first value */
      *++sp = vals[1];
             
      /* Finally null the end of the stack */
      NULLSTACK (vals + nvalues - sp);
    }
  else
    goto vm_error_no_values;

  /* Restore the last program */
  program = SCM_FRAME_PROGRAM (fp);
  CACHE_PROGRAM ();
  CHECK_IP ();
  NEXT;
}

VM_DEFINE_INSTRUCTION (64, return_values_star, "return/values*", 1, -1, -1)
{
  SCM l;

  nvalues = FETCH ();
  ASSERT (nvalues >= 1);
    
  nvalues--;
  POP (l);
  while (SCM_CONSP (l))
    {
      PUSH (SCM_CAR (l));
      l = SCM_CDR (l);
      nvalues++;
    }
  if (SCM_UNLIKELY (!SCM_NULL_OR_NIL_P (l))) {
    finish_args = scm_list_1 (l);
    goto vm_error_improper_list;
  }

  goto vm_return_values;
}

VM_DEFINE_INSTRUCTION (65, truncate_values, "truncate-values", 2, -1, -1)
{
  SCM x;
  int nbinds, rest;
  POP (x);
  nvalues = scm_to_int (x);
  nbinds = FETCH ();
  rest = FETCH ();

  if (rest)
    nbinds--;

  if (nvalues < nbinds)
    goto vm_error_not_enough_values;

  if (rest)
    POP_LIST (nvalues - nbinds);
  else
    DROPN (nvalues - nbinds);

  NEXT;
}

VM_DEFINE_INSTRUCTION (66, box, "box", 1, 1, 0)
{
  SCM val;
  POP (val);
  SYNC_BEFORE_GC ();
  LOCAL_SET (FETCH (), scm_cell (scm_tc7_variable, SCM_UNPACK (val)));
  NEXT;
}

/* for letrec:
   (let ((a *undef*) (b *undef*) ...)
     (set! a (lambda () (b ...)))
     ...)
 */
VM_DEFINE_INSTRUCTION (67, empty_box, "empty-box", 1, 0, 0)
{
  SYNC_BEFORE_GC ();
  LOCAL_SET (FETCH (),
             scm_cell (scm_tc7_variable, SCM_UNPACK (SCM_UNDEFINED)));
  NEXT;
}

VM_DEFINE_INSTRUCTION (68, local_boxed_ref, "local-boxed-ref", 1, 0, 1)
{
  SCM v = LOCAL_REF (FETCH ());
  ASSERT_BOUND_VARIABLE (v);
  PUSH (VARIABLE_REF (v));
  NEXT;
}

VM_DEFINE_INSTRUCTION (69, local_boxed_set, "local-boxed-set", 1, 1, 0)
{
  SCM v, val;
  v = LOCAL_REF (FETCH ());
  POP (val);
  ASSERT_VARIABLE (v);
  VARIABLE_SET (v, val);
  NEXT;
}

VM_DEFINE_INSTRUCTION (70, free_ref, "free-ref", 1, 0, 1)
{
  scm_t_uint8 idx = FETCH ();
  
  CHECK_FREE_VARIABLE (idx);
  PUSH (FREE_VARIABLE_REF (idx));
  NEXT;
}

/* no free-set -- if a var is assigned, it should be in a box */

VM_DEFINE_INSTRUCTION (71, free_boxed_ref, "free-boxed-ref", 1, 0, 1)
{
  SCM v;
  scm_t_uint8 idx = FETCH ();
  CHECK_FREE_VARIABLE (idx);
  v = FREE_VARIABLE_REF (idx);
  ASSERT_BOUND_VARIABLE (v);
  PUSH (VARIABLE_REF (v));
  NEXT;
}

VM_DEFINE_INSTRUCTION (72, free_boxed_set, "free-boxed-set", 1, 1, 0)
{
  SCM v, val;
  scm_t_uint8 idx = FETCH ();
  POP (val);
  CHECK_FREE_VARIABLE (idx);
  v = FREE_VARIABLE_REF (idx);
  ASSERT_BOUND_VARIABLE (v);
  VARIABLE_SET (v, val);
  NEXT;
}

VM_DEFINE_INSTRUCTION (73, make_closure, "make-closure", 0, 2, 1)
{
  SCM vect;
  POP (vect);
  SYNC_BEFORE_GC ();
  /* fixme underflow */
  *sp = scm_double_cell (scm_tc7_program, (scm_t_bits)SCM_PROGRAM_OBJCODE (*sp),
                         (scm_t_bits)SCM_PROGRAM_OBJTABLE (*sp), (scm_t_bits)vect);
  NEXT;
}

VM_DEFINE_INSTRUCTION (74, make_variable, "make-variable", 0, 0, 1)
{
  SYNC_BEFORE_GC ();
  /* fixme underflow */
  PUSH (scm_cell (scm_tc7_variable, SCM_UNPACK (SCM_UNDEFINED)));
  NEXT;
}

VM_DEFINE_INSTRUCTION (75, fix_closure, "fix-closure", 2, 0, 1)
{
  SCM x, vect;
  unsigned int i = FETCH ();
  i <<= 8;
  i += FETCH ();
  POP (vect);
  /* FIXME CHECK_LOCAL (i) */ 
  x = LOCAL_REF (i);
  /* FIXME ASSERT_PROGRAM (x); */
  SCM_SET_CELL_WORD_3 (x, vect);
  NEXT;
}

VM_DEFINE_INSTRUCTION (76, define, "define", 0, 0, 2)
{
  SCM sym, val;
  POP (sym);
  POP (val);
  SYNC_REGISTER ();
  VARIABLE_SET (scm_sym2var (sym, scm_current_module_lookup_closure (),
                             SCM_BOOL_T),
                val);
  NEXT;
}

VM_DEFINE_INSTRUCTION (77, make_keyword, "make-keyword", 0, 1, 1)
{
  CHECK_UNDERFLOW ();
  SYNC_REGISTER ();
  *sp = scm_symbol_to_keyword (*sp);
  NEXT;
}

VM_DEFINE_INSTRUCTION (78, make_symbol, "make-symbol", 0, 1, 1)
{
  CHECK_UNDERFLOW ();
  SYNC_REGISTER ();
  *sp = scm_string_to_symbol (*sp);
  NEXT;
}


/*
(defun renumber-ops ()
  "start from top of buffer and renumber 'VM_DEFINE_FOO (\n' sequences"
  (interactive "")
  (save-excursion
    (let ((counter -1)) (goto-char (point-min))
      (while (re-search-forward "^VM_DEFINE_[^ ]+ (\\([^,]+\\)," (point-max) t)
        (replace-match
         (number-to-string (setq counter (1+ counter)))
          t t nil 1)))))
*/
/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/
