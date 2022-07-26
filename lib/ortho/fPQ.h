/**
 * @file
 * @brief @ref snode priority queue for @ref shortPath in @ref sgraph
 */

/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#include <ortho/sgraph.h>

#define N_VAL(n) (n)->n_val
#define N_IDX(n) (n)->n_idx
#define N_DAD(n) (n)->n_dad
#define N_EDGE(n) (n)->n_edge
#define E_WT(e) (e->weight)
#define E_INCR(e) (e->incr)

#ifdef INLINEPQ

#include "assert.h"
static snode**  pq;
static int     PQcnt;
static snode    guard;
static int     PQsize;

static void
PQgen(int sz)
{
  if (!pq) {
    pq = N_NEW(sz+1,snode*);
    pq[0] = &guard;
    PQsize = sz;
  }
  PQcnt = 0;
}

static void
PQfree()
{
  free (pq);
  pq = NULL;
  PQcnt = 0;
}

static void
PQinit()
{
  PQcnt = 0;
}

static void
PQcheck ()
{
  int i;
 
  for (i = 1; i <= PQcnt; i++) {
    if (N_IDX(pq[i]) != i) {
      assert (0);
    }
  }
}

static void
PQupheap(int k)
{
  snode* x;
  x = pq[k];
  int     v = x->n_val;
  int     next = k/2;
  snode*  n;
  
  while (N_VAL(n = pq[next]) < v) {
    pq[k] = n;
    N_IDX(n) = k;
    k = next;
    next /= 2;
  }
  pq[k] = x;
  N_IDX(x) = k;
}

static int
PQ_insert(snode* np)
{
  if (PQcnt == PQsize) {
    agerr (AGERR, "Heap overflow\n");
    return 1;
  }
  PQcnt++;
  pq[PQcnt] = np;
  PQupheap (PQcnt);
  PQcheck();
  return 0;
}

static void
PQdownheap (int k)
{
  snode*    x = pq[k];
  int      v = N_VAL(x);
  int      lim = PQcnt/2;
  snode*    n;
  int      j;

  while (k <= lim) {
    j = k+k;
    n = pq[j];
    if (j < PQcnt) {
      if (N_VAL(n) < N_VAL(pq[j+1])) {
        j++;
        n = pq[j];
      }
    }
    if (v >= N_VAL(n)) break;
    pq[k] = n;
    N_IDX(n) = k;
    k = j;
  }
  pq[k] = x;
  N_IDX(x) = k;
}

static snode*
PQremove ()
{
  snode* n;

  if (PQcnt) {
    n = pq[1];
    pq[1] = pq[PQcnt];
    PQcnt--;
    if (PQcnt) PQdownheap (1);
    PQcheck();
    return n;
  }
  else return 0;
}

static void
PQupdate (snode* n, int d)
{
  N_VAL(n) = d;
  PQupheap (n->n_idx);
  PQcheck();
}

static void
PQprint ()
{
  int    i;
  snode*  n;

  fprintf (stderr, "Q: ");
  for (i = 1; i <= PQcnt; i++) {
    n = pq[i];
    fprintf (stderr, "%s(%d:%d) ",  
      n->index, N_IDX(n), N_VAL(n));
  }
  fprintf (stderr, "\n");
}
#else

#ifndef FPQ_H
#define FPQ_H

void PQgen(int sz);
void PQfree(void);
void PQinit(void);
void PQcheck (void);
void PQupheap(int);
int PQ_insert(snode* np);
void PQdownheap (int k);
snode* PQremove (void);
void PQupdate (snode* n, int d);
void PQprint (void);
#endif
#endif
