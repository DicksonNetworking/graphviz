/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/


#include "config.h"
#include <limits.h>
#include <sfdpgen/sfdp.h>
#include <neatogen/neato.h>
#include <neatogen/adjust.h>
#include <pack/pack.h>
#include <assert.h>
#include <ctype.h>
#include <sfdpgen/spring_electrical.h>
#include <neatogen/overlap.h>
#include <sfdpgen/uniform_stress.h>
#include <sfdpgen/stress_model.h>
#include <cgraph/strcasecmp.h>

static void sfdp_init_edge(edge_t * e)
{
    agbindrec(e, "Agedgeinfo_t", sizeof(Agedgeinfo_t), TRUE);	//node custom data
    common_init_edge(e);
}

static void sfdp_init_node_edge(graph_t * g)
{
    node_t *n;
    edge_t *e;

    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
	neato_init_node(n);
    }
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
	for (e = agfstout(g, n); e; e = agnxtout(g, e))
	    sfdp_init_edge(e);
    }
}

static void sfdp_init_graph(Agraph_t * g)
{
    int outdim;

    setEdgeType(g, EDGETYPE_LINE);
    outdim = late_int(g, agfindgraphattr(g, "dimen"), 2, 2);
    GD_ndim(agroot(g)) = late_int(g, agfindgraphattr(g, "dim"), outdim, 2);
    Ndim = GD_ndim(agroot(g)) = MIN(GD_ndim(agroot(g)), MAXDIM);
    GD_odim(agroot(g)) = MIN(outdim, Ndim);
    sfdp_init_node_edge(g);
}

/* getPos:
 */
static real *getPos(Agraph_t * g)
{
    Agnode_t *n;
    real *pos = N_NEW(Ndim * agnnodes(g), real);
    int ix, i;

    if (agfindnodeattr(g, "pos") == NULL)
	return pos;

    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
	i = ND_id(n);
	if (hasPos(n)) {
	    for (ix = 0; ix < Ndim; ix++) {
		pos[i * Ndim + ix] = ND_pos(n)[ix];
	    }
	}
    }

    return pos;
}

static void sfdpLayout(graph_t * g, spring_electrical_control ctrl,
		       int hops, pointf pad)
{
    real *sizes;
    real *pos;
    Agnode_t *n;
    int flag, i;
    int n_edge_label_nodes = 0, *edge_label_nodes = NULL;
    SparseMatrix D = NULL;
    SparseMatrix A;

    if (ctrl->method == METHOD_SPRING_MAXENT) /* maxent can work with distance matrix */
	A = makeMatrix(g, &D);
    else
	A = makeMatrix(g, NULL);

    if (ctrl->overlap >= 0) {
	if (ctrl->edge_labeling_scheme > 0)
	    sizes = getSizes(g, pad, &n_edge_label_nodes, &edge_label_nodes);
	else
	    sizes = getSizes(g, pad, NULL, NULL);
    }
    else
	sizes = NULL;
    pos = getPos(g);

    switch (ctrl->method) {
    case METHOD_SPRING_ELECTRICAL:
    case METHOD_SPRING_MAXENT:
	multilevel_spring_electrical_embedding(Ndim, A, D, ctrl, NULL, sizes, pos, n_edge_label_nodes, edge_label_nodes, &flag);
	break;
    case METHOD_UNIFORM_STRESS:
	uniform_stress(Ndim, A, pos, &flag);
	break;
    case METHOD_STRESS:{
	int maxit = 200;
	real tol = 0.001;
	int weighted = TRUE;

	if (!D){
	    D = SparseMatrix_get_real_adjacency_matrix_symmetrized(A);/* all distance 1 */
	    weighted = FALSE;
	} else {
	    D = SparseMatrix_symmetrize_nodiag(D);
	    weighted = TRUE;
	}
	if (hops > 0){
	    SparseMatrix DD;
	    DD = SparseMatrix_distance_matrix_khops(hops, D, weighted);
	    if (Verbose){
		fprintf(stderr,"extracted a %d-neighborhood graph of %d edges from a graph of %d edges\n",
		    hops, (DD->nz)/2, (D->nz/2));
	    }
	    SparseMatrix_delete(D);
	    D = DD;
	}

	stress_model(Ndim, A, D, &pos, TRUE, maxit, tol, &flag);
	}
	break;
    }

    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
	real *npos = pos + (Ndim * ND_id(n));
	for (i = 0; i < Ndim; i++) {
	    ND_pos(n)[i] = npos[i];
	}
    }

    free(sizes);
    free(pos);
    SparseMatrix_delete (A);
    if (D) SparseMatrix_delete (D);
    FREE(edge_label_nodes);
}

static int
late_smooth (graph_t* g, Agsym_t* sym, int dflt)
{
    char* s;
    int v;
    int rv;

    if (!sym) return dflt;
    s = agxget (g, sym);
    if (isdigit(*s)) {
#if (defined(HAVE_GTS) || defined(HAVE_TRIANGLE))
	if ((v = atoi (s)) <= SMOOTHING_RNG)
#else
	if ((v = atoi (s)) <= SMOOTHING_SPRING)
#endif
	    rv = v;
	else
	    rv = dflt;
    }
    else if (isalpha(*s)) {
	if (!strcasecmp(s, "avg_dist"))
	    rv = SMOOTHING_STRESS_MAJORIZATION_AVG_DIST;
	else if (!strcasecmp(s, "graph_dist"))
	    rv = SMOOTHING_STRESS_MAJORIZATION_GRAPH_DIST;
	else if (!strcasecmp(s, "none"))
	    rv = SMOOTHING_NONE;
	else if (!strcasecmp(s, "power_dist"))
	    rv = SMOOTHING_STRESS_MAJORIZATION_POWER_DIST;
#if (defined(HAVE_GTS) || defined(HAVE_TRIANGLE))
	else if (!strcasecmp(s, "rng"))
	    rv = SMOOTHING_RNG;
#endif
	else if (!strcasecmp(s, "spring"))
	    rv = SMOOTHING_SPRING;
#if (defined(HAVE_GTS) || defined(HAVE_TRIANGLE))
	else if (!strcasecmp(s, "triangle"))
	    rv = SMOOTHING_TRIANGLE;
#endif
	else
	    rv = dflt;
    }
    else
	rv = dflt;
    return rv;
}

static int
late_quadtree_scheme (graph_t* g, Agsym_t* sym, int dflt)
{
    char* s;
    int v;
    int rv;

    if (!sym) return dflt;
    s = agxget (g, sym);
    if (isdigit(*s)) {
      if ((v = atoi (s)) <= QUAD_TREE_FAST && v >= QUAD_TREE_NONE){
	rv = v;
      }	else {
	rv = dflt;
      }
    } else if (isalpha(*s)) {
      if (!strcasecmp(s, "none") || !strcasecmp(s, "false") ){
	rv = QUAD_TREE_NONE;
      } else if (!strcasecmp(s, "normal") || !strcasecmp(s, "true") || !strcasecmp(s, "yes")){
	rv = QUAD_TREE_NORMAL;
      } else if (!strcasecmp(s, "fast")){
	rv = QUAD_TREE_FAST;
      }	else {
	rv = dflt;
      }
    } else {
      rv = dflt;
    }
    return rv;
}


/* tuneControl:
 * Use user values to reset control
 * 
 * Possible parameters:
 *   ctrl->use_node_weights
 */
static void
tuneControl (graph_t* g, spring_electrical_control ctrl)
{
    long seed;
    int init;

    seed = ctrl->random_seed;
    init = setSeed (g, INIT_RANDOM, &seed);
    if (init != INIT_RANDOM) {
        agerr(AGWARN, "sfdp only supports start=random\n");
    }
    ctrl->random_seed = seed;

    ctrl->K = late_double(g, agfindgraphattr(g, "K"), -1.0, 0.0);
    ctrl->p = -1.0*late_double(g, agfindgraphattr(g, "repulsiveforce"), -AUTOP, 0.0);
    ctrl->multilevels = late_int(g, agfindgraphattr(g, "levels"), INT_MAX, 0);
    ctrl->smoothing = late_smooth(g, agfindgraphattr(g, "smoothing"), SMOOTHING_NONE);
    ctrl->tscheme = late_quadtree_scheme(g, agfindgraphattr(g, "quadtree"), QUAD_TREE_NORMAL);
    ctrl->method = METHOD_SPRING_ELECTRICAL;
    ctrl->beautify_leaves = mapBool (agget(g, "beautify"), FALSE);
    ctrl->do_shrinking = mapBool (agget(g, "overlap_shrink"), TRUE);
    ctrl->rotation = late_double(g, agfindgraphattr(g, "rotation"), 0.0, -MAXDOUBLE);
    ctrl->edge_labeling_scheme = late_int(g, agfindgraphattr(g, "label_scheme"), 0, 0);
    if (ctrl->edge_labeling_scheme > 4) {
	agerr (AGWARN, "label_scheme = %d > 4 : ignoring\n", ctrl->edge_labeling_scheme);
	ctrl->edge_labeling_scheme = 0;
    }
}

void sfdp_layout(graph_t * g)
{
    int doAdjust;
    adjust_data am;
    int hops = -1;
    sfdp_init_graph(g);
    doAdjust = (Ndim == 2);

    if (agnnodes(g)) {
	Agraph_t **ccs;
	Agraph_t *sg;
	int ncc;
	int i;
	expand_t sep;
	pointf pad;
	spring_electrical_control ctrl = spring_electrical_control_new();

	tuneControl (g, ctrl);
#if (defined(HAVE_GTS) || defined(HAVE_TRIANGLE))
	graphAdjustMode(g, &am, "prism0");
#else
	graphAdjustMode(g, &am, 0);
#endif

	pad.x = PS2INCH(DFLT_MARGIN);
	pad.y = PS2INCH(DFLT_MARGIN);

	if ((am.mode == AM_PRISM) && doAdjust) {
	    doAdjust = 0;  /* overlap removal done in sfdp */
	    ctrl->overlap = am.value;
    	    ctrl->initial_scaling = am.scaling;
	    sep = sepFactor(g);
	    if (sep.doAdd) {
		pad.x = PS2INCH(sep.x);
		pad.y = PS2INCH(sep.y);
	    }
	}
	else {
   		/* Turn off overlap removal in sfdp if prism not used */
	    ctrl->overlap = -1;
	}

	if (Verbose)
	    spring_electrical_control_print(ctrl);

	ccs = ccomps(g, &ncc, 0);
	if (ncc == 1) {
	    sfdpLayout(g, ctrl, hops, pad);
	    if (doAdjust) removeOverlapWith(g, &am);
	    spline_edges(g);
	} else {
	    pack_info pinfo;
	    getPackInfo(g, l_node, CL_OFFSET, &pinfo);
	    pinfo.doSplines = 1;

	    for (i = 0; i < ncc; i++) {
		sg = ccs[i];
		nodeInduce(sg);
		sfdpLayout(sg, ctrl, hops, pad);
		if (doAdjust) removeOverlapWith(sg, &am);
		setEdgeType(sg, EDGETYPE_LINE);
		spline_edges(sg);
	    }
	    packSubgraphs(ncc, ccs, g, &pinfo);
	}
	for (i = 0; i < ncc; i++) {
	    agdelete(g, ccs[i]);
	}
	free(ccs);
	spring_electrical_control_delete(ctrl);
    }

    dotneato_postprocess(g);
}

void sfdp_cleanup(graph_t * g)
{
    node_t *n;
    edge_t *e;

    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
	for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
	    gv_cleanup_edge(e);
	}
	gv_cleanup_node(n);
    }
}
 
