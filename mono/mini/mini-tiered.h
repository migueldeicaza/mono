/**
 * \file
 *
 * Tiered compilation definitions
 *
 * Copyright 2019 Microsoft
 * Licensed under the MIT license. See LICENSE file in the project root for full license information.
 */

#ifndef __MONO_MINI_TIERED_H__
#define __MONO_MINI_TIERED_H__

#include "mini.h"

void mini_tiered_emit_entry (MonoCompile *cfg);
void mini_tiered_dump       (void);
void mini_tiered_rejit      (MonoMethod *method, void *tier0code);
void mini_tiered_shutdown   (void);

#endif /* __MONO_MINI_TIERED_H__ */

