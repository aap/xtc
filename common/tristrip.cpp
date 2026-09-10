/*
 * Triangle strips by tunneling.  After A. J. Stewart, "Tunneling for
 * Triangle Strips in Continuous Level-of-Detail Meshes" (2001), and
 * aap's librw sketch of it.
 *
 * The strip set S is a set of edges of the dual graph in which every
 * node has at most two, and there is no cycle: a set of paths.  A
 * tunnel is a path n0..nk between two strip ends (nodes with fewer than
 * two strip edges) whose edges alternate: not in S, in S, ..., not in S.
 * Complementing its edges keeps every interior node's degree and gives
 * the two ends one more, so S stays a set of paths with one edge more,
 * that is one path fewer -- unless a cycle closed.  That is checked by
 * walking from the tunnel's nodes after the complement, and undone if
 * so.  Tunnels are found breadth first, shortest first, from every end.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tristrip.h"

#ifndef nil
#define nil NULL
#endif

typedef struct Edge Edge;
struct Edge {
	int32 node;		/* the node across this edge */
	uint8 connected;
	uint8 other;		/* this edge's index in that node */
	uint8 strip;		/* in S */
};

typedef struct Node Node;
struct Node {
	int32 v[3];		/* edge k is (v[k], v[k+1]) */
	Edge e[3];
	int32 parent;		/* tunnel search: node we came from */
	uint8 parentEdge;	/* and its edge that led here */
	uint8 visited;
	uint8 done;		/* greedy strips: assigned; output: emitted */
};

typedef struct Mesh Mesh;
struct Mesh {
	int32 numNodes;
	Node *nodes;
	int32 *queue;		/* the search's, numNodes long */
};

#define NEXT(x) (((x)+1) % 3)
#define PREV(x) (((x)+2) % 3)

static int
stripDegree(Node *n)
{
	return n->e[0].strip + n->e[1].strip + n->e[2].strip;
}

static int
numConnections(Node *n)
{
	return n->e[0].connected + n->e[1].connected + n->e[2].connected;
}

/* a strip end: room for one more strip edge, and somewhere to go */
static int
isEnd(Node *n)
{
	return numConnections(n) > 0 && stripDegree(n) < 2;
}

static void
complementEdge(Mesh *m, Node *n, int i)
{
	Edge *e = &n->e[i];
	e->strip = !e->strip;
	m->nodes[e->node].e[e->other].strip = !m->nodes[e->node].e[e->other].strip;
}


/*
 * The dual graph: nodes sharing an edge with opposite direction (the
 * same winding on both sides) get connected.  A hash of the directed
 * edges keeps this linear.
 */

typedef struct HashEnt HashEnt;
struct HashEnt {
	int32 a, b;	/* the directed edge, -1: empty */
	int32 node;
	int32 edge;
};

static uint32
hashEdge(int32 a, int32 b, uint32 mask)
{
	uint32 h = (uint32)a*2654435761u ^ ((uint32)b*40503u + 0x9e3779b9u);
	h ^= h >> 15;
	return h & mask;
}

static void
connectNodes(Mesh *m)
{
	uint32 size, mask, h;
	HashEnt *tab;
	Node *n, *nn;
	int32 i, j;

	for(size = 16; size < (uint32)m->numNodes*6; size *= 2);
	mask = size-1;
	tab = (HashEnt*)malloc(size*sizeof(HashEnt));
	for(h = 0; h < size; h++)
		tab[h].a = -1;

	/* every directed edge goes in; a duplicate (non manifold) is left
	 * out, the first one wins */
	for(i = 0; i < m->numNodes; i++){
		n = &m->nodes[i];
		for(j = 0; j < 3; j++){
			int32 a = n->v[j], b = n->v[NEXT(j)];
			h = hashEdge(a, b, mask);
			while(tab[h].a >= 0 && !(tab[h].a == a && tab[h].b == b))
				h = (h+1) & mask;
			if(tab[h].a < 0){
				tab[h].a = a;
				tab[h].b = b;
				tab[h].node = i;
				tab[h].edge = j;
			}
		}
	}

	/* and each edge looks for its reverse */
	for(i = 0; i < m->numNodes; i++){
		n = &m->nodes[i];
		for(j = 0; j < 3; j++){
			int32 a = n->v[NEXT(j)], b = n->v[j];
			if(n->e[j].connected)
				continue;
			h = hashEdge(a, b, mask);
			while(tab[h].a >= 0 && !(tab[h].a == a && tab[h].b == b))
				h = (h+1) & mask;
			if(tab[h].a < 0 || tab[h].node == i)
				continue;
			nn = &m->nodes[tab[h].node];
			if(nn->e[tab[h].edge].connected)
				continue;
			n->e[j].node = tab[h].node;
			n->e[j].other = tab[h].edge;
			n->e[j].connected = 1;
			nn->e[tab[h].edge].node = i;
			nn->e[tab[h].edge].other = j;
			nn->e[tab[h].edge].connected = 1;
		}
	}
	free(tab);
}


/*
 * Greedy strips: from each unassigned node, keep going to a free
 * neighbour.  Prefers the neighbour with the fewest free neighbours of
 * its own, the SGI heuristic, so corners get used up first.
 */

static int
numFree(Mesh *m, Node *n)
{
	int i, k = 0;
	for(i = 0; i < 3; i++)
		if(n->e[i].connected && !m->nodes[n->e[i].node].done)
			k++;
	return k;
}

static void
buildStrips(Mesh *m)
{
	Node *n, *nn, *best;
	int32 i, j, bestEdge, f, bestFree;

	for(i = 0; i < m->numNodes; i++){
		n = &m->nodes[i];
		if(n->done)
			continue;
		n->done = 1;
		for(;;){
			best = nil;
			bestEdge = -1;
			bestFree = 4;
			for(j = 0; j < 3; j++){
				if(!n->e[j].connected)
					continue;
				nn = &m->nodes[n->e[j].node];
				if(nn->done)
					continue;
				f = numFree(m, nn);
				if(f < bestFree){
					bestFree = f;
					best = nn;
					bestEdge = j;
				}
			}
			if(best == nil)
				break;
			best->done = 1;
			complementEdge(m, n, bestEdge);
			n = best;
		}
	}
}


/*
 * Tunneling
 */

/* from node t along the strip edge i: 1 if the walk comes back to t */
static int
closesLoop(Mesh *m, Node *t, int i)
{
	Node *n;
	int last, j;

	last = t->e[i].other;
	n = &m->nodes[t->e[i].node];
	for(;;){
		if(n == t)
			return 1;
		for(j = 0; j < 3; j++)
			if(n->e[j].strip && j != last)
				break;
		if(j == 3)
			return 0;
		last = n->e[j].other;
		n = &m->nodes[n->e[j].node];
	}
}

/* complement the edges from end back to the search's start */
static void
complementPath(Mesh *m, Node *end)
{
	Node *n;
	for(n = end; n->parent >= 0; n = &m->nodes[n->parent])
		complementEdge(m, &m->nodes[n->parent], n->parentEdge);
}

/* apply the tunnel that ends at end; undo it if it closed a loop */
static int
applyTunnel(Mesh *m, Node *end)
{
	Node *n;
	int i;

	complementPath(m, end);
	/* a cycle would have to use a new edge, so it passes through a
	 * node of the tunnel; every such node with two strip edges gets
	 * walked once */
	for(n = end;; n = &m->nodes[n->parent]){
		if(stripDegree(n) == 2){
			for(i = 0; i < 3; i++)
				if(n->e[i].strip)
					break;
			if(closesLoop(m, n, i)){
				complementPath(m, end);
				return 0;
			}
		}
		if(n->parent < 0)
			break;
	}
	return 1;
}

/* breadth first from start: non-strip edges on even depths, strip
 * edges on odd ones.  A strip end reached on a non-strip edge ends the
 * tunnel, if that does not close a loop; else the search goes on
 * through it. */
static int
findTunnel(Mesh *m, Node *start, int maxLen)
{
	int32 head, tail, depth, i;
	int edgetype, found;
	Node *n, *nn;
	int32 *queue = m->queue;
	int32 *depths;

	found = 0;
	head = tail = 0;
	start->visited = 1;
	start->parent = -1;
	queue[tail++] = start - m->nodes;
	/* depth is recovered by counting parents; cheap enough for the
	 * lengths we search */
	while(head < tail && !found){
		n = &m->nodes[queue[head++]];
		depth = 0;
		for(nn = n; nn->parent >= 0; nn = &m->nodes[nn->parent])
			depth++;
		if(depth >= maxLen)
			continue;
		edgetype = depth & 1;
		for(i = 0; i < 3; i++){
			if(!n->e[i].connected || n->e[i].strip != edgetype)
				continue;
			nn = &m->nodes[n->e[i].node];
			if(nn->visited)
				continue;
			nn->visited = 1;
			nn->parent = n - m->nodes;
			nn->parentEdge = i;
			queue[tail++] = nn - m->nodes;
			if(edgetype == 0 && isEnd(nn) && applyTunnel(m, nn)){
				found = 1;
				break;
			}
		}
	}
	(void)depths;
	for(i = 0; i < tail; i++)
		m->nodes[queue[i]].visited = 0;
	return found;
}

static int
tunnel(Mesh *m, int maxTunnel)
{
	static const int lengths[] = { 3, 7, 13, 21, 31 };
	int32 i, len, l, improved, total;

	total = 0;
	for(l = 0; l < (int)(sizeof(lengths)/sizeof(lengths[0])) + 1; l++){
		len = l < (int)(sizeof(lengths)/sizeof(lengths[0])) ? lengths[l] : maxTunnel;
		if(len > maxTunnel)
			len = maxTunnel;
		do{
			improved = 0;
			for(i = 0; i < m->numNodes; i++){
				if(!isEnd(&m->nodes[i]))
					continue;
				if(findTunnel(m, &m->nodes[i], len)){
					improved++;
					total++;
				}
			}
		}while(improved);
		if(len == maxTunnel)
			break;
	}
	return total;
}


/*
 * Output: walk every path, alternate the winding with swaps where the
 * strip turns the same way twice, stitch the paths with two repeated
 * indices.  After librw's makeMesh.
 */

static int
nextEdge(Node *n, int last)
{
	int i;
	for(i = 0; i < 3; i++)
		if(n->e[i].strip && i != last)
			return i;
	return -1;
}

static int32*
makeStrip(Mesh *m, int32 *numIndices, int32 *numStrips)
{
	int32 *idx, num, cap;
	int32 k, i, j, seam;
	int even, rightturn, lastrightturn;
	Node *n;

	/* three indices and two for the stitch per triangle is the worst */
	cap = m->numNodes*5 + 2;
	idx = (int32*)malloc(cap*sizeof(int32));
	num = 0;
	even = 1;
	*numStrips = 0;

	for(k = 0; k < m->numNodes; k++){
		n = &m->nodes[k];
		if(n->done || stripDegree(n) >= 2)
			continue;
		(*numStrips)++;
		j = nextEdge(n, -1);
		if(j < 0){
			/* a lone triangle */
			n->done = 1;
			if(num > 0){
				idx[num] = idx[num-1];
				num++;
				idx[num++] = n->v[!even];
			}
			idx[num++] = n->v[!even];
			idx[num++] = n->v[even];
			idx[num++] = n->v[2];
			even = !even;
			continue;
		}

		/* i is the edge we enter a triangle through, j the exit */
		seam = num;
		if(seam)
			num += 2;
		if(even){
			i = PREV(j);
			idx[num++] = n->v[i];
			idx[num++] = n->v[NEXT(i)];
		}else{
			i = NEXT(j);
			idx[num++] = n->v[NEXT(i)];
			idx[num++] = n->v[i];
		}
		lastrightturn = -1;
		while(j >= 0){
			n->done = 1;
			rightturn = NEXT(i) == j;
			if(rightturn == lastrightturn){
				/* the same turn twice: repeat a vertex */
				idx[num] = idx[num-2];
				num++;
				even = !even;
			}
			lastrightturn = rightturn;
			idx[num++] = rightturn ? n->v[NEXT(j)] : n->v[j];
			even = !even;

			i = n->e[j].other;
			n = &m->nodes[n->e[j].node];
			j = nextEdge(n, i);
		}
		n->done = 1;
		idx[num++] = n->v[PREV(i)];
		even = !even;
		if(seam){
			idx[seam] = idx[seam-1];
			idx[seam+1] = idx[seam+2];
		}
	}
	assert(num <= cap);
	*numIndices = num;
	return idx;
}


int*
xTriStrip(int numTris, const int *tris, int maxTunnel, xStripStats *stats, int *numIndices)
{
	Mesh m;
	int32 i, numStrips, numTunnels;
	int32 *idx;

	m.numNodes = numTris;
	m.nodes = (Node*)malloc(numTris*sizeof(Node));
	m.queue = (int32*)malloc(numTris*sizeof(int32));
	memset(m.nodes, 0, numTris*sizeof(Node));
	for(i = 0; i < numTris; i++){
		m.nodes[i].v[0] = tris[3*i+0];
		m.nodes[i].v[1] = tris[3*i+1];
		m.nodes[i].v[2] = tris[3*i+2];
	}

	connectNodes(&m);
	buildStrips(&m);
	numTunnels = maxTunnel > 0 ? tunnel(&m, maxTunnel) : 0;

	for(i = 0; i < numTris; i++)
		m.nodes[i].done = 0;
	idx = makeStrip(&m, numIndices, &numStrips);

	if(stats){
		stats->numTris = numTris;
		stats->numStrips = numStrips;
		stats->numTunnels = numTunnels;
		stats->numIndices = *numIndices;
	}
	free(m.nodes);
	free(m.queue);
	return idx;
}


/*
 * Verification: every non-degenerate triangle of the strip is a source
 * triangle with the same winding, each source triangle drawn once.
 */

typedef struct Tri Tri;
struct Tri {
	int32 a, b, c;	/* rotated so a is the smallest */
	int32 idx;
};

static void
canon(int32 a, int32 b, int32 c, Tri *t)
{
	if(b < a && b <= c){ int32 x = a; a = b; b = c; c = x; }
	else if(c < a && c < b){ int32 x = a; a = c; c = b; b = x; }
	t->a = a; t->b = b; t->c = c;
}

static int
cmpTri(const void *p, const void *q)
{
	const Tri *s = (const Tri*)p, *t = (const Tri*)q;
	if(s->a != t->a) return s->a < t->a ? -1 : 1;
	if(s->b != t->b) return s->b < t->b ? -1 : 1;
	if(s->c != t->c) return s->c < t->c ? -1 : 1;
	return 0;
}

int
xTriStripVerify(int numTris, const int *tris, int numIndices, const int *strip)
{
	Tri *src, key, *f;
	uint8 *seen;
	int32 i, a, b, c, ok, lo, hi, mid;

	src = (Tri*)malloc(numTris*sizeof(Tri));
	seen = (uint8*)calloc(numTris, 1);
	for(i = 0; i < numTris; i++){
		canon(tris[3*i], tris[3*i+1], tris[3*i+2], &src[i]);
		src[i].idx = i;
	}
	qsort(src, numTris, sizeof(Tri), cmpTri);

	ok = 1;
	for(i = 0; i + 2 < numIndices && ok; i++){
		if(i & 1){ a = strip[i+1]; b = strip[i]; }
		else{ a = strip[i]; b = strip[i+1]; }
		c = strip[i+2];
		if(a == b || b == c || a == c)
			continue;
		canon(a, b, c, &key);
		/* the first of the equal run, then the first unseen of it */
		lo = 0; hi = numTris;
		while(lo < hi){
			mid = (lo+hi)/2;
			if(cmpTri(&src[mid], &key) < 0) lo = mid+1;
			else hi = mid;
		}
		f = nil;
		for(; lo < numTris && cmpTri(&src[lo], &key) == 0; lo++)
			if(!seen[src[lo].idx]){
				f = &src[lo];
				break;
			}
		if(f == nil){
			printf("tristrip: triangle %d %d %d at %d not in the source (or twice)\n", a, b, c, i);
			ok = 0;
			break;
		}
		seen[f->idx] = 1;
	}
	for(i = 0; i < numTris && ok; i++)
		if(!seen[i]){
			printf("tristrip: source triangle %d not in the strip\n", i);
			ok = 0;
		}
	free(src);
	free(seen);
	return ok;
}
