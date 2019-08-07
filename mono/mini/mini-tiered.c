/*
 * mini-tiered.c: Support for tiered compilation
 *
 * Licensed to the .NET Foundation under one or more agreements.
 * The .NET Foundation licenses this file to you under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#include <config.h>
#include "../metadata/threads-types.h"
#include "mini-tiered.h"
#include "ir-emit.h"

typedef struct {
	int counter;
	MonoMethod *method;
	void *tiered_code;
	int rejit_requested;
} TieredStatusSlot;

static mono_mutex_t tiered_queue_lock;
// Counters for the number of invocations
static TieredStatusSlot *tiered_counters;
// Number of counters to create
static int ntiered = 4096;
static TieredStatusSlot *next_tiered;
static GSList *rejit_queue;

static mono_cond_t rejit_wait;
static mono_mutex_t rejit_mutex;

//
// FIXME: seems like I dont need the method now, since I have the slot with the data.
void
mini_tiered_rejit (MonoMethod *method, void *_slot)
{
	TieredStatusSlot *slot = _slot;
	gboolean wakeup = FALSE;
	
	mono_os_mutex_lock (&tiered_queue_lock);
	if (!slot->rejit_requested){
		slot->rejit_requested = 1;
		rejit_queue = g_slist_append (rejit_queue, slot);
		wakeup = TRUE;
	}
	mono_os_mutex_unlock (&tiered_queue_lock);
	if (wakeup)
		mono_os_cond_signal (&rejit_wait);

	//printf ("Requested Rejit %s at %p\n", mono_method_full_name (method, TRUE), slot->tiered_code);
}

static void
recompiler_thread (void *arg)
{
	while (TRUE){
		mono_os_cond_wait (&rejit_wait, &rejit_mutex);

		if (mono_runtime_is_shutting_down ())
			break;
		
		mono_os_mutex_lock (&tiered_queue_lock);
		int count;
		GSList *items = rejit_queue;
		rejit_queue = NULL;
		mono_os_mutex_unlock (&tiered_queue_lock);
		
		for (GSList *start = items; start != NULL; start = start->next){
			TieredStatusSlot *slot = start->data;
			
			printf ("Rejitting: %s\n", mono_method_full_name (slot->method, TRUE));
		}

		mono_os_mutex_unlock (&rejit_mutex);
	}
}

static void
mini_tiered_init ()
{
	MonoError error;
	tiered_counters = g_new0 (TieredStatusSlot, ntiered);
	next_tiered = tiered_counters;

	mono_os_mutex_init (&tiered_queue_lock);
	mono_os_mutex_init (&rejit_mutex);
	mono_os_cond_init (&rejit_wait);
	
	mono_thread_create_internal (mono_domain_get (), recompiler_thread, NULL, MONO_THREAD_CREATE_FLAGS_THREADPOOL, &error);
}

void
mini_tiered_emit_entry (MonoCompile *cfg)
{
	MonoInst *one_ins, *load_ins, *ins;
	MonoBasicBlock *resume_bb;

	// We know that LLVM wont work for this method, do not instrument
	if (cfg->disable_llvm)
		return;

	NEW_BBLOCK (cfg, resume_bb);

	if (tiered_counters == NULL)
		mini_tiered_init ();

	cfg->tier0code = &next_tiered->tiered_code;
	next_tiered->method = cfg->method;
	//printf ("Count for %s is at %p\n", mono_method_full_name (cfg->method, TRUE), next_tiered);

	EMIT_NEW_PCONST (cfg, load_ins, next_tiered);
	EMIT_NEW_ICONST (cfg, one_ins, 1);
	MONO_INST_NEW (cfg, ins, OP_ATOMIC_ADD_I4);
	ins->dreg = mono_alloc_ireg (cfg);
	ins->inst_basereg = load_ins->dreg;
	ins->inst_offset = 0;
	ins->sreg2 = one_ins->dreg;
	ins->type = STACK_I4;
	MONO_ADD_INS (cfg->cbb, ins);

	MONO_EMIT_NEW_BIALU_IMM (cfg, OP_COMPARE_IMM, -1, load_ins->dreg, 100);
	MONO_EMIT_NEW_BRANCH_BLOCK (cfg, OP_PBLT_UN, resume_bb);

	{
		MonoInst *iargs [2];
		
		EMIT_NEW_METHODCONST (cfg, iargs [0], cfg->method);
		EMIT_NEW_PCONST (cfg, iargs [1], next_tiered);
		
		mono_emit_jit_icall (cfg, mini_tiered_rejit, iargs);

	}
	MONO_START_BB (cfg, resume_bb);

	next_tiered++;
}

void
mini_tiered_shutdown ()
{
	mono_os_cond_signal (&rejit_wait);
}

void
mini_tiered_dump ()
{
	TieredStatusSlot *p = tiered_counters;
	for (; p < next_tiered; p++){
		if (p->method == 0)
			break;
		printf ("%d %s\n", p->counter, mono_method_full_name (p->method, TRUE));
	}
	fflush (stdout);
}
