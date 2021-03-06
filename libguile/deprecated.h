/* This file contains definitions for deprecated features.  When you
   deprecate something, move it here when that is feasible.
*/

#ifndef SCM_DEPRECATED_H
#define SCM_DEPRECATED_H

/* Copyright (C) 2003,2004, 2005, 2006, 2007, 2009, 2010, 2011 Free Software Foundation, Inc.
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

#include "libguile/__scm.h"
#include "libguile/strings.h"
#include "libguile/eval.h"
#include "libguile/throw.h"
#include "libguile/iselect.h"

#if (SCM_ENABLE_DEPRECATED == 1)

/* Deprecated 13-05-2011 because it's better just to scm_dynwind_begin.
   That also avoids the temptation to stuff pointers in an SCM.  */

typedef SCM (*scm_t_inner) (void *);
SCM_DEPRECATED SCM scm_internal_dynamic_wind (scm_t_guard before,
                                              scm_t_inner inner,
                                              scm_t_guard after,
                                              void *inner_data,
                                              void *guard_data);


/* Deprecated 15-05-2011 because it's better to be explicit with the
   `return'.  Code is more readable that way.  */
#define SCM_WTA_DISPATCH_0(gf, subr)			        \
  return scm_wta_dispatch_0 ((gf), (subr))
#define SCM_WTA_DISPATCH_1(gf, a1, pos, subr)			\
  return scm_wta_dispatch_1 ((gf), (a1), (pos), (subr))
#define SCM_WTA_DISPATCH_2(gf, a1, a2, pos, subr)          \
  return scm_wta_dispatch_2 ((gf), (a1), (a2), (pos), (subr))
#define SCM_WTA_DISPATCH_N(gf, args, pos, subr)			\
  return scm_wta_dispatch_n ((gf), (args), (pos), (subr))

/* Deprecated 15-05-2011 because this idiom is not very readable.  */
#define SCM_GASSERT0(cond, gf, subr)            \
  if (SCM_UNLIKELY (!(cond)))                   \
    return scm_wta_dispatch_0 ((gf), (subr))
#define SCM_GASSERT1(cond, gf, a1, pos, subr)           \
  if (SCM_UNLIKELY (!(cond)))                           \
    return scm_wta_dispatch_1 ((gf), (a1), (pos), (subr))
#define SCM_GASSERT2(cond, gf, a1, a2, pos, subr)	\
  if (SCM_UNLIKELY (!(cond)))                           \
    return scm_wta_dispatch_2 ((gf), (a1), (a2), (pos), (subr))
#define SCM_GASSERTn(cond, gf, args, pos, subr)         \
  if (SCM_UNLIKELY (!(cond)))                           \
    return scm_wta_dispatch_n ((gf), (args), (pos), (subr))

/* Deprecated 15-05-2011 because this is a one-off macro that does
   strange things.  */
#define SCM_WTA_DISPATCH_1_SUBR(subr, a1, pos)				\
  return (SCM_UNPACK ((*SCM_SUBR_GENERIC (subr)))			\
	  ? scm_call_1 ((*SCM_SUBR_GENERIC (subr)), (a1))               \
	  : (scm_i_wrong_type_arg_symbol (SCM_SUBR_NAME (subr), (pos), (a1)), SCM_UNSPECIFIED))

#define SCM_LIST0 SCM_EOL
#define SCM_LIST1(e0) scm_cons ((e0), SCM_EOL)
#define SCM_LIST2(e0, e1) scm_cons2 ((e0), (e1), SCM_EOL)
#define SCM_LIST3(e0, e1, e2) scm_cons ((e0), SCM_LIST2 ((e1), (e2)))
#define SCM_LIST4(e0, e1, e2, e3)\
     scm_cons2 ((e0), (e1), SCM_LIST2 ((e2), (e3)))
#define SCM_LIST5(e0, e1, e2, e3, e4)\
     scm_cons ((e0), SCM_LIST4 ((e1), (e2), (e3), (e4)))
#define SCM_LIST6(e0, e1, e2, e3, e4, e5)\
     scm_cons2 ((e0), (e1), SCM_LIST4 ((e2), (e3), (e4), (e5)))
#define SCM_LIST7(e0, e1, e2, e3, e4, e5, e6)\
     scm_cons ((e0), SCM_LIST6 ((e1), (e2), (e3), (e4), (e5), (e6)))
#define SCM_LIST8(e0, e1, e2, e3, e4, e5, e6, e7)\
     scm_cons2 ((e0), (e1), SCM_LIST6 ((e2), (e3), (e4), (e5), (e6), (e7)))
#define SCM_LIST9(e0, e1, e2, e3, e4, e5, e6, e7, e8)\
     scm_cons ((e0),\
	       SCM_LIST8 ((e1), (e2), (e3), (e4), (e5), (e6), (e7), (e8)))

#define SCM_OPDIRP SCM_OPDIRP__GONE__REPLACE_WITH__SCM_DIRP_and_SCM_DIR_OPEN_P
#define SCM_PROCEDURE SCM_PROCEDURE__GONE__REPLACE_WITH__scm_procedure
#define SCM_PROCEDURE_WITH_SETTER_P SCM_PROCEDURE_WITH_SETTER_P__GONE__REPLACE_WITH__scm_is_true__scm_procedure_with_setter_p
#define SCM_SETTER SCM_SETTER__GONE__REPLACE_WITH__scm_setter
#define SCM_THREAD_SWITCHING_CODE SCM_THREAD_SWITCHING_CODE__GONE__REMOVE_FROM_YOUR_CODE
#define SCM_VALIDATE_NUMBER_COPY SCM_VALIDATE_NUMBER_COPY__GONE__REPLACE_WITH__SCM_VALIDATE_DOUBLE_COPY
#define SCM_VALIDATE_NUMBER_DEF_COPY SCM_VALIDATE_NUMBER_DEF_COPY__GONE__REPLACE_WITH__SCM_UNBNDP_and_SCM_VALIDATE_DOUBLE_COPY
#define SCM_VALIDATE_OPDIR SCM_VALIDATE_OPDIR__GONE
#define SCM_VALIDATE_STRING_COPY SCM_VALIDATE_STRING_COPY__GONE
#define SCM_VALIDATE_SUBSTRING_SPEC_COPY SCM_VALIDATE_SUBSTRING_SPEC_COPY__GONE
#define scm_array scm_array__GONE__REPLACE_WITH__scm_t_array
#define scm_array_dim scm_array_dim__GONE__REPLACE_WITH__scm_t_array_dim
#define scm_async_click scm_async_click__GONE__REPLACE_WITH__scm_async_tick
#define scm_call_generic_0 scm_call_generic_0__GONE__REPLACE_WITH__scm_call_0
#define scm_call_generic_1 scm_call_generic_1__GONE__REPLACE_WITH__scm_call_1
#define scm_call_generic_2 scm_call_generic_2__GONE__REPLACE_WITH__scm_call_2
#define scm_call_generic_3 scm_call_generic_3__GONE__REPLACE_WITH__scm_call_3
#define scm_apply_generic scm_apply_generic__GONE__REPLACE_WITH__scm_apply_0
#define scm_fport scm_fport__GONE__REPLACE_WITH__scm_t_fport
#define scm_listify scm_listify__GONE__REPLACE_WITH__scm_list_n
#define scm_option scm_option__GONE__REPLACE_WITH__scm_t_option
#define scm_port scm_port__GONE__REPLACE_WITH__scm_t_port
#define scm_port_rw_active scm_port_rw_active__GONE__REPLACE_WITH__scm_t_port_rw_active
#define scm_ptob_descriptor scm_ptob_descriptor__GONE__REPLACE_WITH__scm_t_ptob_descriptor
#define scm_rng scm_rng__GONE__REPLACE_WITH__scm_t_rng
#define scm_rstate scm_rstate__GONE__REPLACE_WITH__scm_t_rstate
#define scm_sizet scm_sizet__GONE__REPLACE_WITH__size_t
#define scm_srcprops scm_srcprops__GONE__REPLACE_WITH__scm_t_srcprops
#define scm_srcprops_chunk scm_srcprops_chunk__GONE__REPLACE_WITH__scm_t_srcprops_chunk
#define scm_struct_i_flags scm_struct_i_flags__GONE__REPLACE_WITH__scm_vtable_index_flags
#define scm_struct_i_free scm_struct_i_free__GONE__REPLACE_WITH__scm_vtable_index_instance_finalize
#define scm_subr_entry scm_subr_entry__GONE__REPLACE_WITH__scm_t_subr_entry
#define scm_substring_move_left_x scm_substring_move_left_x__GONE__REPLACE_WITH__scm_substring_move_x
#define scm_substring_move_right_x scm_substring_move_right_x__GONE__REPLACE_WITH__scm_substring_move_x
#define scm_vtable_index_printer scm_vtable_index_printer__GONE__REPLACE_WITH__scm_vtable_index_instance_printer
#define scm_vtable_index_vtable scm_vtable_index_vtable__GONE__REPLACE_WITH__scm_vtable_index_self
typedef scm_i_t_array scm_i_t_array__GONE__REPLACE_WITH__scm_t_array;

#ifndef BUILDING_LIBGUILE
#define SCM_ASYNC_TICK  SCM_ASYNC_TICK__GONE__REPLACE_WITH__scm_async_tick
#endif




/* Deprecated 26-05-2011, as the GC_STUBBORN API doesn't do anything any
   more.  */
SCM_API SCM scm_immutable_cell (scm_t_bits car, scm_t_bits cdr);
SCM_API SCM scm_immutable_double_cell (scm_t_bits car, scm_t_bits cbr,
				       scm_t_bits ccr, scm_t_bits cdr);



void scm_i_init_deprecated (void);

#endif

#endif /* SCM_DEPRECATED_H */
