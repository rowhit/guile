/* Copyright (C) 1998,2000,2001,2002,2003,2004,2006,2007,2008 Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */



#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdarg.h>

#include "libguile/_scm.h"

#include "libguile/eval.h"
#include "libguile/smob.h"
#include "libguile/procprop.h"
#include "libguile/vectors.h"
#include "libguile/hashtab.h"
#include "libguile/struct.h"
#include "libguile/variable.h"
#include "libguile/fluids.h"
#include "libguile/deprecation.h"

#include "libguile/modules.h"

int scm_module_system_booted_p = 0;

scm_t_bits scm_module_tag;

static SCM the_module;

static SCM the_root_module_var;

static SCM
the_root_module ()
{
  if (scm_module_system_booted_p)
    return SCM_VARIABLE_REF (the_root_module_var);
  else
    return SCM_BOOL_F;
}

SCM_DEFINE (scm_current_module, "current-module", 0, 0, 0,
	    (),
	    "Return the current module.")
#define FUNC_NAME s_scm_current_module
{
  SCM curr = scm_fluid_ref (the_module);

  return scm_is_true (curr) ? curr : the_root_module ();
}
#undef FUNC_NAME

static void scm_post_boot_init_modules (void);

SCM_DEFINE (scm_set_current_module, "set-current-module", 1, 0, 0,
	    (SCM module),
	    "Set the current module to @var{module} and return\n"
	    "the previous current module.")
#define FUNC_NAME s_scm_set_current_module
{
  SCM old;

  if (!scm_module_system_booted_p)
    scm_post_boot_init_modules ();

  SCM_VALIDATE_MODULE (SCM_ARG1, module);

  old = scm_current_module ();
  scm_fluid_set_x (the_module, module);

  return old;
}
#undef FUNC_NAME

SCM_DEFINE (scm_interaction_environment, "interaction-environment", 0, 0, 0,
	    (),
	    "Return a specifier for the environment that contains\n"
	    "implementation--defined bindings, typically a superset of those\n"
	    "listed in the report.  The intent is that this procedure will\n"
	    "return the environment in which the implementation would\n"
	    "evaluate expressions dynamically typed by the user.")
#define FUNC_NAME s_scm_interaction_environment
{
  return scm_current_module ();
}
#undef FUNC_NAME

SCM
scm_c_call_with_current_module (SCM module,
				SCM (*func)(void *), void *data)
{
  return scm_c_with_fluid (the_module, module, func, data);
}

void
scm_dynwind_current_module (SCM module)
{
  scm_dynwind_fluid (the_module, module);
}

/*
  convert "A B C" to scheme list (A B C)
 */
static SCM
convert_module_name (const char *name)
{
  SCM list = SCM_EOL;
  SCM *tail = &list;

  const char *ptr;
  while (*name)
    {
      while (*name == ' ')
	name++;
      ptr = name;
      while (*ptr && *ptr != ' ')
	ptr++;
      if (ptr > name)
	{
	  SCM sym = scm_from_locale_symboln (name, ptr-name);
	  *tail = scm_cons (sym, SCM_EOL);
	  tail = SCM_CDRLOC (*tail);
	}
      name = ptr;
    }

  return list;
}

static SCM process_define_module_var;
static SCM process_use_modules_var;
static SCM resolve_module_var;

SCM
scm_c_resolve_module (const char *name)
{
  return scm_resolve_module (convert_module_name (name));
}

SCM
scm_resolve_module (SCM name)
{
  return scm_call_1 (SCM_VARIABLE_REF (resolve_module_var), name);
}

SCM
scm_c_define_module (const char *name,
		     void (*init)(void *), void *data)
{
  SCM module = scm_call_1 (SCM_VARIABLE_REF (process_define_module_var),
			   scm_list_1 (convert_module_name (name)));
  if (init)
    scm_c_call_with_current_module (module, (SCM (*)(void*))init, data);
  return module;
}

void
scm_c_use_module (const char *name)
{
  scm_call_1 (SCM_VARIABLE_REF (process_use_modules_var),
	      scm_list_1 (scm_list_1 (convert_module_name (name))));
}

static SCM module_export_x_var;

SCM
scm_module_export (SCM module, SCM namelist)
{
  return scm_call_2 (SCM_VARIABLE_REF (module_export_x_var),
		     module, namelist);
}


/*
  @code{scm_c_export}(@var{name-list})

  @code{scm_c_export} exports the named bindings from the current
  module, making them visible to users of the module. This function
  takes a list of string arguments, terminated by NULL, e.g.

  @example
    scm_c_export ("add-double-record", "bamboozle-money", NULL);
  @end example
*/
void
scm_c_export (const char *name, ...)
{
  if (name)
    {
      va_list ap;
      SCM names = scm_cons (scm_from_locale_symbol (name), SCM_EOL);
      SCM *tail = SCM_CDRLOC (names);
      va_start (ap, name);
      while (1)
	{
	  const char *n = va_arg (ap, const char *);
	  if (n == NULL)
	    break;
	  *tail = scm_cons (scm_from_locale_symbol (n), SCM_EOL);
	  tail = SCM_CDRLOC (*tail);
	}
      va_end (ap);
      scm_module_export (scm_current_module (), names);
    }
}


/* Environments */

SCM
scm_top_level_env (SCM thunk)
{
  if (SCM_IMP (thunk))
    return SCM_EOL;
  else
    return scm_cons (thunk, SCM_EOL);
}

SCM
scm_env_top_level (SCM env)
{
  while (scm_is_pair (env))
    {
      SCM car_env = SCM_CAR (env);
      if (!scm_is_pair (car_env) && scm_is_true (scm_procedure_p (car_env)))
	return car_env;
      env = SCM_CDR (env);
    }
  return SCM_BOOL_F;
}

SCM_SYMBOL (sym_module, "module");

SCM
scm_lookup_closure_module (SCM proc)
{
  if (scm_is_false (proc))
    return the_root_module ();
  else if (SCM_EVAL_CLOSURE_P (proc))
    return SCM_PACK (SCM_SMOB_DATA (proc));
  else
    {
      SCM mod;

      /* FIXME: The `module' property is no longer set.  See
	 `set-module-eval-closure!' in `boot-9.scm'.  */
      abort ();

      mod = scm_procedure_property (proc, sym_module);
      if (scm_is_false (mod))
	mod = the_root_module ();
      return mod;
    }
}

SCM_DEFINE (scm_env_module, "env-module", 1, 0, 0,
	    (SCM env),
	    "Return the module of @var{ENV}, a lexical environment.")
#define FUNC_NAME s_scm_env_module
{
  return scm_lookup_closure_module (scm_env_top_level (env));
}
#undef FUNC_NAME

/*
 * C level implementation of the standard eval closure
 *
 * This increases loading speed substantially.  The code may be
 * replaced by something based on environments.[ch], in a future
 * release.
 */

/* The `module-make-local-var!' variable.  */
static SCM module_make_local_var_x_var = SCM_UNSPECIFIED;

/* The `default-duplicate-binding-procedures' variable.  */
static SCM default_duplicate_binding_procedures_var = SCM_UNSPECIFIED;

/* Return the list of default duplicate binding handlers (procedures).  */
static inline SCM
default_duplicate_binding_handlers (void)
{
  SCM get_handlers;

  get_handlers = SCM_VARIABLE_REF (default_duplicate_binding_procedures_var);

  return (scm_call_0 (get_handlers));
}

/* Resolve the import of SYM in MODULE, where SYM is currently provided by
   both IFACE1 as VAR1 and IFACE2 as VAR2.  Return the variable chosen by the
   duplicate binding handlers or `#f'.  */
static inline SCM
resolve_duplicate_binding (SCM module, SCM sym,
			   SCM iface1, SCM var1,
			   SCM iface2, SCM var2)
{
  SCM result = SCM_BOOL_F;

  if (!scm_is_eq (var1, var2))
    {
      SCM val1, val2;
      SCM handlers, h, handler_args;

      val1 = SCM_VARIABLE_REF (var1);
      val2 = SCM_VARIABLE_REF (var2);

      val1 = (val1 == SCM_UNSPECIFIED) ? SCM_BOOL_F : val1;
      val2 = (val2 == SCM_UNSPECIFIED) ? SCM_BOOL_F : val2;

      handlers = SCM_MODULE_DUPLICATE_HANDLERS (module);
      if (scm_is_false (handlers))
	handlers = default_duplicate_binding_handlers ();

      handler_args = scm_list_n (module, sym,
				 iface1, val1, iface2, val2,
				 var1, val1,
				 SCM_UNDEFINED);

      for (h = handlers;
	   scm_is_pair (h) && scm_is_false (result);
	   h = SCM_CDR (h))
	{
	  result = scm_apply (SCM_CAR (h), handler_args, SCM_EOL);
	}
    }
  else
    result = var1;

  return result;
}

/* Lookup SYM as an imported variable of MODULE.  */
static inline SCM
module_imported_variable (SCM module, SCM sym)
{
#define SCM_BOUND_THING_P scm_is_true
  register SCM var, imports;

  /* Search cached imported bindings.  */
  imports = SCM_MODULE_IMPORT_OBARRAY (module);
  var = scm_hashq_ref (imports, sym, SCM_UNDEFINED);
  if (SCM_BOUND_THING_P (var))
    return var;

  {
    /* Search the use list for yet uncached imported bindings, possibly
       resolving duplicates as needed and caching the result in the import
       obarray.  */
    SCM uses;
    SCM found_var = SCM_BOOL_F, found_iface = SCM_BOOL_F;

    for (uses = SCM_MODULE_USES (module);
	 scm_is_pair (uses);
	 uses = SCM_CDR (uses))
      {
	SCM iface;

	iface = SCM_CAR (uses);
	var = scm_module_variable (iface, sym);

	if (SCM_BOUND_THING_P (var))
	  {
	    if (SCM_BOUND_THING_P (found_var))
	      {
		/* SYM is a duplicate binding (imported more than once) so we
		   need to resolve it.  */
		found_var = resolve_duplicate_binding (module, sym,
						       found_iface, found_var,
						       iface, var);
		if (scm_is_eq (found_var, var))
		  found_iface = iface;
	      }
	    else
	      /* Keep track of the variable we found and check for other
		 occurences of SYM in the use list.  */
	      found_var = var, found_iface = iface;
	  }
      }

    if (SCM_BOUND_THING_P (found_var))
      {
	/* Save the lookup result for future reference.  */
	(void) scm_hashq_set_x (imports, sym, found_var);
	return found_var;
      }
  }

  return SCM_BOOL_F;
#undef SCM_BOUND_THING_P
}

SCM_DEFINE (scm_module_local_variable, "module-local-variable", 2, 0, 0,
	    (SCM module, SCM sym),
	    "Return the variable bound to @var{sym} in @var{module}.  Return "
	    "@code{#f} is @var{sym} is not bound locally in @var{module}.")
#define FUNC_NAME s_scm_module_local_variable
{
#define SCM_BOUND_THING_P(b) \
  (scm_is_true (b))

  register SCM b;

  /* SCM_MODULE_TAG is not initialized yet when `boot-9.scm' is being
     evaluated.  */
  if (scm_module_system_booted_p)
    SCM_VALIDATE_MODULE (1, module);

  SCM_VALIDATE_SYMBOL (2, sym);


  /* 1. Check module obarray */
  b = scm_hashq_ref (SCM_MODULE_OBARRAY (module), sym, SCM_UNDEFINED);
  if (SCM_BOUND_THING_P (b))
    return b;

  /* 2. Search imported bindings.  In order to be consistent with
     `module-variable', the binder gets called only when no imported binding
     matches SYM.  */
  b = module_imported_variable (module, sym);
  if (SCM_BOUND_THING_P (b))
    return SCM_BOOL_F;

  {
    /* 3. Query the custom binder.  */
    SCM binder = SCM_MODULE_BINDER (module);

    if (scm_is_true (binder))
      {
	b = scm_call_3 (binder, module, sym, SCM_BOOL_F);
	if (SCM_BOUND_THING_P (b))
	  return b;
      }
  }

  return SCM_BOOL_F;

#undef SCM_BOUND_THING_P
}
#undef FUNC_NAME

SCM_DEFINE (scm_module_variable, "module-variable", 2, 0, 0,
	    (SCM module, SCM sym),
	    "Return the variable bound to @var{sym} in @var{module}.  This "
	    "may be both a local variable or an imported variable.  Return "
	    "@code{#f} is @var{sym} is not bound in @var{module}.")
#define FUNC_NAME s_scm_module_variable
{
#define SCM_BOUND_THING_P(b) \
  (scm_is_true (b))

  register SCM var;

  if (scm_module_system_booted_p)
    SCM_VALIDATE_MODULE (1, module);

  SCM_VALIDATE_SYMBOL (2, sym);

  /* 1. Check module obarray */
  var = scm_hashq_ref (SCM_MODULE_OBARRAY (module), sym, SCM_UNDEFINED);
  if (SCM_BOUND_THING_P (var))
    return var;

  /* 2. Search among the imported variables.  */
  var = module_imported_variable (module, sym);
  if (SCM_BOUND_THING_P (var))
    return var;

  {
    /* 3. Query the custom binder.  */
    SCM binder;

    binder = SCM_MODULE_BINDER (module);
    if (scm_is_true (binder))
      {
	var = scm_call_3 (binder, module, sym, SCM_BOOL_F);
	if (SCM_BOUND_THING_P (var))
	  return var;
      }
  }

  return SCM_BOOL_F;

#undef SCM_BOUND_THING_P
}
#undef FUNC_NAME

scm_t_bits scm_tc16_eval_closure;

#define SCM_F_EVAL_CLOSURE_INTERFACE (1<<16)
#define SCM_EVAL_CLOSURE_INTERFACE_P(e) \
  (SCM_CELL_WORD_0 (e) & SCM_F_EVAL_CLOSURE_INTERFACE)

/* NOTE: This function may be called by a smob application
   or from another C function directly. */
SCM
scm_eval_closure_lookup (SCM eclo, SCM sym, SCM definep)
{
  SCM module = SCM_PACK (SCM_SMOB_DATA (eclo));
  if (scm_is_true (definep))
    {
      if (SCM_EVAL_CLOSURE_INTERFACE_P (eclo))
	return SCM_BOOL_F;
      return scm_call_2 (SCM_VARIABLE_REF (module_make_local_var_x_var),
			 module, sym);
    }
  else
    return scm_module_variable (module, sym);
}

SCM_DEFINE (scm_standard_eval_closure, "standard-eval-closure", 1, 0, 0,
	    (SCM module),
	    "Return an eval closure for the module @var{module}.")
#define FUNC_NAME s_scm_standard_eval_closure
{
  SCM_RETURN_NEWSMOB (scm_tc16_eval_closure, SCM_UNPACK (module));
}
#undef FUNC_NAME


SCM_DEFINE (scm_standard_interface_eval_closure,
	    "standard-interface-eval-closure", 1, 0, 0,
	    (SCM module),
	    "Return a interface eval closure for the module @var{module}. "
	    "Such a closure does not allow new bindings to be added.")
#define FUNC_NAME s_scm_standard_interface_eval_closure
{
  SCM_RETURN_NEWSMOB (scm_tc16_eval_closure | SCM_F_EVAL_CLOSURE_INTERFACE,
		      SCM_UNPACK (module));
}
#undef FUNC_NAME

SCM
scm_module_lookup_closure (SCM module)
{
  if (scm_is_false (module))
    return SCM_BOOL_F;
  else
    return SCM_MODULE_EVAL_CLOSURE (module);
}

SCM
scm_current_module_lookup_closure ()
{
  if (scm_module_system_booted_p)
    return scm_module_lookup_closure (scm_current_module ());
  else
    return SCM_BOOL_F;
}

SCM
scm_module_transformer (SCM module)
{
  if (scm_is_false (module))
    return SCM_BOOL_F;
  else
    return SCM_MODULE_TRANSFORMER (module);
}

SCM
scm_current_module_transformer ()
{
  if (scm_module_system_booted_p)
    return scm_module_transformer (scm_current_module ());
  else
    return SCM_BOOL_F;
}

SCM_DEFINE (scm_module_import_interface, "module-import-interface", 2, 0, 0,
	    (SCM module, SCM sym),
	    "Return the module or interface from which @var{sym} is imported "
	    "in @var{module}.  If @var{sym} is not imported (i.e., it is not "
	    "defined in @var{module} or it is a module-local binding instead "
	    "of an imported one), then @code{#f} is returned.")
#define FUNC_NAME s_scm_module_import_interface
{
  SCM var, result = SCM_BOOL_F;

  SCM_VALIDATE_MODULE (1, module);
  SCM_VALIDATE_SYMBOL (2, sym);

  var = scm_module_variable (module, sym);
  if (scm_is_true (var))
    {
      /* Look for the module that provides VAR.  */
      SCM local_var;

      local_var = scm_hashq_ref (SCM_MODULE_OBARRAY (module), sym,
				 SCM_UNDEFINED);
      if (scm_is_eq (local_var, var))
	result = module;
      else
	{
	  /* Look for VAR among the used modules.  */
	  SCM uses, imported_var;

	  for (uses = SCM_MODULE_USES (module);
	       scm_is_pair (uses) && scm_is_false (result);
	       uses = SCM_CDR (uses))
	    {
	      imported_var = scm_module_variable (SCM_CAR (uses), sym);
	      if (scm_is_eq (imported_var, var))
		result = SCM_CAR (uses);
	    }
	}
    }

  return result;
}
#undef FUNC_NAME

/* scm_sym2var
 *
 * looks up the variable bound to SYM according to PROC.  PROC should be
 * a `eval closure' of some module.
 *
 * When no binding exists, and DEFINEP is true, create a new binding
 * with a initial value of SCM_UNDEFINED.  Return `#f' when DEFINEP as
 * false and no binding exists.
 *
 * When PROC is `#f', it is ignored and the binding is searched for in
 * the scm_pre_modules_obarray (a `eq' hash table).
 */

SCM scm_pre_modules_obarray;

SCM 
scm_sym2var (SCM sym, SCM proc, SCM definep)
#define FUNC_NAME "scm_sym2var"
{
  SCM var;

  if (SCM_NIMP (proc))
    {
      if (SCM_EVAL_CLOSURE_P (proc))
	{
	  /* Bypass evaluator in the standard case. */
	  var = scm_eval_closure_lookup (proc, sym, definep);
	}
      else
	var = scm_call_2 (proc, sym, definep);
    }
  else
    {
      SCM handle;

      if (scm_is_false (definep))
	var = scm_hashq_ref (scm_pre_modules_obarray, sym, SCM_BOOL_F);
      else
	{
	  handle = scm_hashq_create_handle_x (scm_pre_modules_obarray,
					      sym, SCM_BOOL_F);
	  var = SCM_CDR (handle);
	  if (scm_is_false (var))
	    {
	      var = scm_make_variable (SCM_UNDEFINED);
	      SCM_SETCDR (handle, var);
	    }
	}
    }

  if (scm_is_true (var) && !SCM_VARIABLEP (var))
    SCM_MISC_ERROR ("~S is not bound to a variable", scm_list_1 (sym));

  return var;
}
#undef FUNC_NAME

SCM
scm_c_module_lookup (SCM module, const char *name)
{
  return scm_module_lookup (module, scm_from_locale_symbol (name));
}

SCM
scm_module_lookup (SCM module, SCM sym)
#define FUNC_NAME "module-lookup"
{
  SCM var;
  SCM_VALIDATE_MODULE (1, module);

  var = scm_sym2var (sym, scm_module_lookup_closure (module), SCM_BOOL_F);
  if (scm_is_false (var))
    SCM_MISC_ERROR ("unbound variable: ~S", scm_list_1 (sym));
  return var;
}
#undef FUNC_NAME

SCM
scm_c_lookup (const char *name)
{
  return scm_lookup (scm_from_locale_symbol (name));
}

SCM
scm_lookup (SCM sym)
{
  SCM var = 
    scm_sym2var (sym, scm_current_module_lookup_closure (), SCM_BOOL_F);
  if (scm_is_false (var))
    scm_misc_error ("scm_lookup", "unbound variable: ~S", scm_list_1 (sym));
  return var;
}

SCM
scm_c_module_define (SCM module, const char *name, SCM value)
{
  return scm_module_define (module, scm_from_locale_symbol (name), value);
}

SCM
scm_module_define (SCM module, SCM sym, SCM value)
#define FUNC_NAME "module-define"
{
  SCM var;
  SCM_VALIDATE_MODULE (1, module);

  var = scm_sym2var (sym, scm_module_lookup_closure (module), SCM_BOOL_T);
  SCM_VARIABLE_SET (var, value);
  return var;
}
#undef FUNC_NAME

SCM
scm_c_define (const char *name, SCM value)
{
  return scm_define (scm_from_locale_symbol (name), value);
}

SCM
scm_define (SCM sym, SCM value)
{
  SCM var =
    scm_sym2var (sym, scm_current_module_lookup_closure (), SCM_BOOL_T);
  SCM_VARIABLE_SET (var, value);
  return var;
}

SCM_DEFINE (scm_module_reverse_lookup, "module-reverse-lookup", 2, 0, 0,
	    (SCM module, SCM variable),
	    "Return the symbol under which @var{variable} is bound in "
	    "@var{module} or @var{#f} if @var{variable} is not visible "
	    "from @var{module}.  If @var{module} is @code{#f}, then the "
	    "pre-module obarray is used.")
#define FUNC_NAME s_scm_module_reverse_lookup
{
  SCM obarray;
  long i, n;

  if (scm_is_false (module))
    obarray = scm_pre_modules_obarray;
  else
    {
      SCM_VALIDATE_MODULE (1, module);
      obarray = SCM_MODULE_OBARRAY (module);
    }

  if (!SCM_HASHTABLE_P (obarray))
      return SCM_BOOL_F;

  /* XXX - We do not use scm_hash_fold here to avoid searching the
     whole obarray.  We should have a scm_hash_find procedure. */

  n = SCM_HASHTABLE_N_BUCKETS (obarray);
  for (i = 0; i < n; ++i)
    {
      SCM ls = SCM_HASHTABLE_BUCKET (obarray, i), handle;
      while (!scm_is_null (ls))
	{
	  handle = SCM_CAR (ls);

	  if (SCM_CAR (handle) == SCM_PACK (NULL))
	    {
	      /* FIXME: We hit a weak pair whose car has become unreachable.
		 We should remove the pair in question or something.  */
	    }
	  else
	    {
	      if (SCM_CDR (handle) == variable)
		return SCM_CAR (handle);
	    }

	  ls = SCM_CDR (ls);
	}
    }

  /* Try the `uses' list.  */
  {
    SCM uses = SCM_MODULE_USES (module);
    while (scm_is_pair (uses))
      {
	SCM sym = scm_module_reverse_lookup (SCM_CAR (uses), variable);
	if (scm_is_true (sym))
	  return sym;
	uses = SCM_CDR (uses);
      }
  }

  return SCM_BOOL_F;
}
#undef FUNC_NAME

SCM_DEFINE (scm_get_pre_modules_obarray, "%get-pre-modules-obarray", 0, 0, 0,
	    (),
	    "Return the obarray that is used for all new bindings before "
	    "the module system is booted.  The first call to "
	    "@code{set-current-module} will boot the module system.")
#define FUNC_NAME s_scm_get_pre_modules_obarray
{
  return scm_pre_modules_obarray;
}
#undef FUNC_NAME

SCM_SYMBOL (scm_sym_system_module, "system-module");

SCM
scm_system_module_env_p (SCM env)
{
  SCM proc = scm_env_top_level (env);
  if (scm_is_false (proc))
    return SCM_BOOL_T;
  return ((scm_is_true (scm_procedure_property (proc,
						scm_sym_system_module)))
	  ? SCM_BOOL_T
	  : SCM_BOOL_F);
}

void
scm_modules_prehistory ()
{
  scm_pre_modules_obarray 
    = scm_permanent_object (scm_c_make_hash_table (1533));
}

void
scm_init_modules ()
{
#include "libguile/modules.x"
  module_make_local_var_x_var = scm_c_define ("module-make-local-var!",
					    SCM_UNDEFINED);
  scm_tc16_eval_closure = scm_make_smob_type ("eval-closure", 0);
  scm_set_smob_apply (scm_tc16_eval_closure, scm_eval_closure_lookup, 2, 0, 0);

  the_module = scm_permanent_object (scm_make_fluid ());
}

static void
scm_post_boot_init_modules ()
{
#define PERM(x) scm_permanent_object(x)

  SCM module_type = SCM_VARIABLE_REF (scm_c_lookup ("module-type"));
  scm_module_tag = (SCM_CELL_WORD_1 (module_type) + scm_tc3_struct);

  resolve_module_var = PERM (scm_c_lookup ("resolve-module"));
  process_define_module_var = PERM (scm_c_lookup ("process-define-module"));
  process_use_modules_var = PERM (scm_c_lookup ("process-use-modules"));
  module_export_x_var = PERM (scm_c_lookup ("module-export!"));
  the_root_module_var = PERM (scm_c_lookup ("the-root-module"));
  default_duplicate_binding_procedures_var =
    PERM (scm_c_lookup ("default-duplicate-binding-procedures"));

  scm_module_system_booted_p = 1;
}

/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/
