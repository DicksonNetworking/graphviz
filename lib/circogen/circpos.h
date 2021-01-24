/* vim:set shiftwidth=4 ts=8: */

/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at http://www.graphviz.org/
 *************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CIRCPOS_H
#define CIRCPOS_H

#include <render.h>
#include <circogen/circular.h>

    extern void circPos(Agraph_t * g, block_t * sn, circ_state *);

#endif

#ifdef __cplusplus
}
#endif
