/*
 * mini-tiered.c: Support for tiered compilation
 *
 * Licensed to the .NET Foundation under one or more agreements.
 * The .NET Foundation licenses this file to you under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#include <config.h>
#include "mini-tiered.h"
#include "ir-emit.h"

typedef struct {
	int counter;
	MonoMethod *method;
} CounterSlot;
	
// Counters for the number of invocations
static CounterSlot *tiered_counters;
// Number of counters to create
static int ntiered = 4096;
static CounterSlot *next_tiered;
static int n;

void
mini_tiered_emit_entry (MonoCompile *cfg)
{
	MonoInst *one_ins, *load_ins, *ins, *reload;
	int countreg;
	MonoBasicBlock *resume_bb;

	NEW_BBLOCK (cfg, resume_bb);

	if (tiered_counters == NULL){
		tiered_counters = g_new0 (CounterSlot, ntiered);
		next_tiered = tiered_counters;
	}
	next_tiered->method = cfg->method;
	printf ("Lock for %s is at %p %d\n", cfg->method->name, next_tiered, n);

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
		EMIT_NEW_PCONST (cfg, iargs [1], NULL);
		
		mono_emit_jit_icall (cfg, mono_trace_enter_method, iargs);
	}
	MONO_START_BB (cfg, resume_bb);

	next_tiered++;
}

void
mini_tiered_dump ()
{
	CounterSlot *p = tiered_counters;
	
	for (int i = 0; i < n; i++){
		if (p->method == 0)
			break;
		printf ("Method %s %d\n", p->method->name, p->counter);
	}
	fflush (STDOUT);
}
