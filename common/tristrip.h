/*
 * Triangle strips from a triangle list.
 *
 * The dual graph of the mesh (a node per triangle, an edge per shared
 * edge) is covered with paths: first greedily, then improved with
 * Stewart's tunnel operator, which finds a path alternating non-strip
 * and strip edges between two strip ends and complements it, joining
 * strips.  The result is one index list: the strips stitched with
 * degenerate triangles, the winding of every source triangle preserved
 * (swaps where a strip does not alternate).
 */

#ifndef TRISTRIP_H
#define TRISTRIP_H

#include "xtcplat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xStripStats xStripStats;
struct xStripStats {
	int numTris;
	int numStrips;	/* paths in the dual graph, before stitching */
	int numTunnels;	/* successful tunnels */
	int numIndices;	/* of the stitched result */
};

/*
 * tris: 3*numTris vertex indices.  Returns the malloc'd stitched strip
 * and its length in *numIndices.  maxTunnel is the longest tunnel to
 * look for, in dual graph edges; 0 leaves the greedy strips as they are.
 * stats may be nil.
 */
int *xTriStrip(int numTris, const int *tris, int maxTunnel, xStripStats *stats, int *numIndices);

/* 1 if the strip draws exactly the triangles of the list, same winding */
int xTriStripVerify(int numTris, const int *tris, int numIndices, const int *strip);

#ifdef __cplusplus
}
#endif

#endif
