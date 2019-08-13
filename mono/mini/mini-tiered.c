

/*
 * mini-tiered.c: Support for tiered compilation
 *
 * Licensed to the .NET Foundation under one or more agreements.
 * The .NET Foundation licenses this file to you under the MIT license.
 * See the LICENSE file in the project root for more information.
 *
 * TODO: resize the TieredStatusSlot block
 *
 * When the flag MONO_OPT_TIER0 is used as an optimization, tiered compilation
 * will take place.   This is achieved by instrumenting the entry point of qualifying
 * methods to track the number of times they have been invoked, and when this count
 * is reached the method is queued for recompilation.
 *
 * We keep a background thread around that will recompile the methods and install
 * the methods once compiled. 
 */

#include <config.h>
#include <mono/metadata/threads-types.h>
#include <mono/utils/mono-logger-internals.h>
#include "mini-runtime.h"
#include "mini-tiered.h"
#include "ir-emit.h"

/* The number of times before a method will be queued for being recompiled */
#define REJIT_THRESHOLD 100

/*
 * This structure describes a method that has been instrumented for tiered compilation.
 *
 * The first field contains an integer that is incremented on entry by each tiered
 * method and tracks the count.   When this reaches a predetermined number, this will
 * set the "rejit_requested" field to 1, and wake up the recompilation thread.
 *
 * the 
 *
 * The structure tracks the domain where this should be compiled, and the method to
 * be compiled.
 */
typedef struct {
	/* Tracks the number of invocations to the method */
	int counter;

	/* The method handle */
	MonoMethod *method;

	/* The domain for which this method was compiled */
	MonoDomain *target_domain;

	/* If set, we have determined that we should rejit this method */
	int rejit_requested;

	/* After a compilation step, this will point to the new code, or NULL on failure */
	uint8_t *new_code;
	
	/*
	 * This slot is updated by the MonoCompile method with the final address of the
	 * generated code elsewhere.  That way we know how to go back and patch the
	 * code
	 */
	uint8_t *tiered_code;
} TieredStatusSlot;

/* This locks protects the rejit_queue */
static mono_mutex_t tiered_queue_lock;

static mono_mutex_t tiered_updates;

/* The queue of methods to be rejited */
static GSList *rejit_queue;

/* Our tiered slots */
static TieredStatusSlot *tiered_methods;

/* Next available slot in the tiered_methods array to populate */
static TieredStatusSlot *next_tiered;

// Number of counters to create
static int ntiered = 4096;

/* Mutex and condition variable to wakeup the rejit method */
static mono_cond_t rejit_wait;
static mono_mutex_t rejit_mutex;

static int tiered_verbose;
/*
 * A call to this method is inlined in the prologue of tiered compilation methods
 */
void
mini_tiered_rejit (MonoMethod *method, void *_slot)
{
	// FIXME: the first parameter is probably redundant now, since we can it from the slot
	TieredStatusSlot *slot = _slot;
	gboolean wakeup = FALSE;
	
	mono_os_mutex_lock (&tiered_queue_lock);
	if (!slot->rejit_requested){
		g_assert (slot->method);
		if (slot->method == NULL)
			return;
		slot->rejit_requested = 1;
		rejit_queue = g_slist_append (rejit_queue, slot);
		wakeup = TRUE;
	}
	mono_os_mutex_unlock (&tiered_queue_lock);
	if (wakeup){
		mono_os_cond_signal (&rejit_wait);
		mono_trace (G_LOG_LEVEL_DEBUG, MONO_TRACE_TIERED, "Tiered-hot method: queuing '%s' at %p hit-count=%d\n", mono_method_full_name (method, TRUE), slot->tiered_code, slot->counter);
	}
}

/*
 * Recompiles hot methods in the background.
 * 
 * This waits for work to be done, and once it is triggered, it attempts to batch compiler
 * all the queued work.   Once that happens, it stops the world, and installs the new
 * addresses, and resumes execution.
 */
static void
recompiler_thread (void *arg)
{
	MonoInternalThread *internal = mono_thread_internal_current ();
	internal->state |= ThreadState_Background;
	internal->flags |= MONO_THREAD_FLAG_DONT_MANAGE;
	mono_native_thread_set_name (mono_native_thread_id_get (), "Tiered Recompilation Thread");
	
	while (TRUE){
		mono_os_cond_wait (&rejit_wait, &rejit_mutex);

		if (mono_runtime_is_shutting_down ())
			break;

		/*
		 * Makes a copy of our homework
		 */
		mono_os_mutex_lock (&tiered_queue_lock);
		GSList *items = rejit_queue;
		rejit_queue = NULL;
		mono_os_mutex_unlock (&tiered_queue_lock);
		
		MonoDomain *domain = mono_domain_get ();
		for (GSList *start = items; start != NULL; start = start->next){
			MonoError err;
			TieredStatusSlot *slot = start->data;
			void *new_code_addr;

			mono_loader_lock ();
			mono_domain_lock(domain);
			new_code_addr = mono_jit_compile_method_llvmjit_only (slot->method, &err);
			mono_domain_unlock (domain);
			mono_loader_unlock ();

			if (is_ok (&err)){
				slot->new_code = new_code_addr;
				mono_trace (G_LOG_LEVEL_DEBUG, MONO_TRACE_TIERED,
					    "Tiered-replaced: %s method at %p with %p\n", mono_method_full_name (slot->method, TRUE), slot->tiered_code, slot->new_code);
			} else {
				slot->new_code = NULL;
				mono_trace (G_LOG_LEVEL_DEBUG, MONO_TRACE_TIERED, "Tiered-error rejiting: %s\n", mono_error_get_message (&err));
			}
		}

		//
		// Now pause the threads, and patch all the addresses
		//
		mono_gc_stop_world ();
		for (GSList *start = items; start != NULL; start = start->next){
			TieredStatusSlot *slot = start->data;
			uint8_t *ptr = slot->tiered_code;

#if defined(TARGET_AMD64)
			amd64_jump_code (ptr, slot->new_code);
#else
# error Tiered compilation not working here
#endif
		}		
		mono_gc_restart_world ();

		mono_os_mutex_unlock (&rejit_mutex);

		g_slist_free (items);
	}
}

/*
 * Initializes the tiered compilation system.
 */
void
mini_tiered_init ()
{
	MonoError error;
	
	tiered_methods = g_new0 (TieredStatusSlot, ntiered);
	next_tiered = tiered_methods;

	mono_os_mutex_init (&tiered_queue_lock);
	mono_os_mutex_init (&tiered_updates);
	mono_os_mutex_init (&rejit_mutex);
	mono_os_cond_init (&rejit_wait);
		
	mono_thread_create_internal (mono_domain_get (), recompiler_thread, NULL, MONO_THREAD_CREATE_FLAGS_THREADPOOL, &error);
}

/*
 * Invoked during compilation to insert the tracking code in the prologue of the method
 */
void
mini_tiered_emit_entry (MonoCompile *cfg)
{
	MonoInst *one_ins, *load_ins, *ins;
	MonoBasicBlock *resume_bb;
	TieredStatusSlot *slot;
	// We know that LLVM wont work for this method, do not instrument
	if (cfg->disable_llvm)
		return;

	tiered_verbose = cfg->verbose_level;

	if (tiered_methods == NULL){
		mini_tiered_init ();
	}
	
	mono_os_mutex_lock (&tiered_updates);
	if (next_tiered < (tiered_methods+ntiered)){
		slot = next_tiered;
		next_tiered++;
	} else
		slot = NULL;
	mono_os_mutex_unlock (&tiered_updates);

	if (slot == NULL)
		return;

	
	cfg->tier0code = &slot->tiered_code;
	slot->method = cfg->method;
	slot->target_domain = mono_domain_get ();

	EMIT_NEW_PCONST (cfg, load_ins, slot);
	EMIT_NEW_ICONST (cfg, one_ins, 1);
	MONO_INST_NEW (cfg, ins, OP_ATOMIC_ADD_I4);
	ins->dreg = mono_alloc_ireg (cfg);
	ins->inst_basereg = load_ins->dreg;
	ins->inst_offset = 0;
	ins->sreg2 = one_ins->dreg;
	ins->type = STACK_I4;
	MONO_ADD_INS (cfg->cbb, ins);

	MONO_EMIT_NEW_BIALU_IMM (cfg, OP_COMPARE_IMM, -1, ins->dreg, REJIT_THRESHOLD);

	NEW_BBLOCK (cfg, resume_bb);
	MONO_EMIT_NEW_BRANCH_BLOCK (cfg, OP_PBLT_UN, resume_bb);

	MonoInst *iargs [2];
	EMIT_NEW_METHODCONST (cfg, iargs [0], cfg->method);
	EMIT_NEW_PCONST (cfg, iargs [1], slot);
		
	mono_emit_jit_icall (cfg, mini_tiered_rejit, iargs);
	MONO_START_BB (cfg, resume_bb);
}

/*
 * Did not seem to work
 */
void
mini_tiered_shutdown ()
{
	if (tiered_methods != NULL)
		mono_os_cond_signal (&rejit_wait);
}

/*
 * Debugging aid
 */
void
mini_tiered_dump ()
{
	TieredStatusSlot *p = tiered_methods;
	for (; p < next_tiered; p++){
		if (p->method == 0)
			break;
		printf ("%d %s\n", p->counter, mono_method_full_name (p->method, TRUE));
	}
	fflush (stdout);
}
