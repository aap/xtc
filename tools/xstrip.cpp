/*
 * xstrip -- triangle strips for .xm models, and the test bench for the
 * stripper in common/tristrip.cpp.
 *
 *	xstrip [-tunnel N] [-v] model.xm ...          stats per mesh, verified
 *	xstrip [-tunnel N] -o model.strips model.xm   the strips, for xm2dsm.lua
 *
 * Reads only what it needs from the text format: the meshes' vertices
 * and f lines.  Vertices that are equal in every attribute are welded
 * for the stripping (an OBJ import has one per face corner, which would
 * leave nothing to strip) and the output refers to one of each set, so
 * it draws the same.  The .strips file has a line `mesh N COUNT' per
 * mesh, followed by the indices.  Default tunnel length 31; 0 for
 * greedy only.
 *
 *	g++ -O2 -Icommon -Isrc_gl -o build/host/xstrip tools/xstrip.cpp common/tristrip.cpp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tristrip.h"

struct Vertex {
	float v[3], n[3], t[2];
	int c[4];
};

struct Mesh {
	int numTris;
	int *tris;
	int cap;
	int numVerts;
	Vertex *verts;
	int vcap;
	int welded;	/* how many distinct ones */
};

static int
cmpVertex(const void *p, const void *q)
{
	return memcmp(p, q, sizeof(Vertex));
}

/* tris get the index of the first vertex equal to theirs */
static void
weld(Mesh *m)
{
	int i, j, *map, *order;
	Vertex *sorted;

	map = (int*)malloc(m->numVerts*sizeof(int));
	order = (int*)malloc(m->numVerts*sizeof(int));
	sorted = (Vertex*)malloc(m->numVerts*sizeof(Vertex));
	memcpy(sorted, m->verts, m->numVerts*sizeof(Vertex));
	/* sort indices by vertex; stable enough by tie breaking on index */
	for(i = 0; i < m->numVerts; i++) order[i] = i;
	struct Cmp { static int f(const void *p, const void *q, void *arg) {
		Vertex *vs = (Vertex*)arg;
		int r = memcmp(&vs[*(int*)p], &vs[*(int*)q], sizeof(Vertex));
		return r ? r : *(int*)p - *(int*)q; } };
	qsort_r(order, m->numVerts, sizeof(int), Cmp::f, m->verts);
	m->welded = 0;
	for(i = 0; i < m->numVerts; i = j){
		for(j = i; j < m->numVerts && cmpVertex(&m->verts[order[i]], &m->verts[order[j]]) == 0; j++)
			map[order[j]] = order[i];
		m->welded++;
	}
	for(i = 0; i < 3*m->numTris; i++)
		if(m->tris[i] >= 0 && m->tris[i] < m->numVerts)
			m->tris[i] = map[m->tris[i]];
	free(map);
	free(order);
	free(sorted);
}

static int
readMeshes(const char *path, Mesh **meshes)
{
	FILE *f = fopen(path, "r");
	char line[256];
	int nm = 0, cap = 0;
	Mesh *m = NULL, *ms = NULL;
	int a, b, c;

	if(f == NULL){
		fprintf(stderr, "can't read %s\n", path);
		return -1;
	}
	while(fgets(line, sizeof(line), f)){
		char *p = line;
		while(*p == ' ' || *p == '\t') p++;
		if(strncmp(p, "mesh ", 5) == 0){
			if(nm == cap){
				cap = cap ? 2*cap : 8;
				ms = (Mesh*)realloc(ms, cap*sizeof(Mesh));
			}
			m = &ms[nm++];
			memset(m, 0, sizeof(*m));
		}else if(strncmp(p, "endmesh", 7) == 0){
			if(m) weld(m);
			m = NULL;
		}else if(m && p[0] == 'v' && p[1] == ' '){
			if(m->numVerts == m->vcap){
				m->vcap = m->vcap ? 2*m->vcap : 256;
				m->verts = (Vertex*)realloc(m->verts, m->vcap*sizeof(Vertex));
			}
			Vertex *vx = &m->verts[m->numVerts++];
			memset(vx, 0, sizeof(*vx));
			sscanf(p+1, "%f %f %f", &vx->v[0], &vx->v[1], &vx->v[2]);
		}else if(m && m->numVerts && p[0] == 'n' && p[1] == ' '){
			Vertex *vx = &m->verts[m->numVerts-1];
			sscanf(p+1, "%f %f %f", &vx->n[0], &vx->n[1], &vx->n[2]);
		}else if(m && m->numVerts && p[0] == 't' && p[1] == ' '){
			Vertex *vx = &m->verts[m->numVerts-1];
			sscanf(p+1, "%f %f", &vx->t[0], &vx->t[1]);
		}else if(m && m->numVerts && p[0] == 'c' && p[1] == ' '){
			Vertex *vx = &m->verts[m->numVerts-1];
			sscanf(p+1, "%d %d %d %d", &vx->c[0], &vx->c[1], &vx->c[2], &vx->c[3]);
		}else if(m && p[0] == 'f' && sscanf(p+1, "%d %d %d", &a, &b, &c) == 3){
			if(m->numTris == m->cap){
				m->cap = m->cap ? 2*m->cap : 256;
				m->tris = (int*)realloc(m->tris, 3*m->cap*sizeof(int));
			}
			m->tris[3*m->numTris+0] = a;
			m->tris[3*m->numTris+1] = b;
			m->tris[3*m->numTris+2] = c;
			m->numTris++;
		}
	}
	fclose(f);
	*meshes = ms;
	return nm;
}

static double
now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec*1e-9;
}

int
main(int argc, char **argv)
{
	int maxTunnel = 31;
	int verbose = 0;
	const char *out = NULL;
	int i, j, nm, n, ok, allok = 1;
	Mesh *ms;
	xStripStats st, st0;
	int *strip, *strip0;
	double t0, t1;
	FILE *of = NULL;

	for(i = 1; i < argc && argv[i][0] == '-'; i++){
		if(strcmp(argv[i], "-tunnel") == 0) maxTunnel = atoi(argv[++i]);
		else if(strcmp(argv[i], "-o") == 0) out = argv[++i];
		else if(strcmp(argv[i], "-v") == 0) verbose = 1;
		else{
			fprintf(stderr, "usage: xstrip [-tunnel N] [-v] [-o out.strips] model.xm ...\n");
			return 1;
		}
	}
	if(i == argc){
		fprintf(stderr, "usage: xstrip [-tunnel N] [-v] [-o out.strips] model.xm ...\n");
		return 1;
	}
	if(out){
		of = fopen(out, "w");
		if(of == NULL){
			fprintf(stderr, "can't write %s\n", out);
			return 1;
		}
	}else
		printf("%-24s %6s %6s %7s %7s %6s %7s %6s %7s %6s %6s\n", "model", "tris", "verts", "list",
			"greedy", "strips", "tunnel", "strips", "tunnels", "ratio", "secs");

	for(; i < argc; i++){
		nm = readMeshes(argv[i], &ms);
		if(nm < 0)
			continue;
		for(j = 0; j < nm; j++){
			Mesh *m = &ms[j];
			char name[64];
			const char *base = strrchr(argv[i], '/');
			base = base ? base+1 : argv[i];
			if(nm > 1) snprintf(name, sizeof(name), "%s:%d", base, j);
			else snprintf(name, sizeof(name), "%s", base);
			if(m->numTris == 0)
				continue;

			t0 = now();
			strip = xTriStrip(m->numTris, m->tris, maxTunnel, &st, &n);
			t1 = now();
			ok = xTriStripVerify(m->numTris, m->tris, n, strip);
			allok &= ok;
			if(of){
				fprintf(of, "mesh %d %d\n", j, n);
				for(int k = 0; k < n; k++)
					fprintf(of, "%d%c", strip[k], (k % 16 == 15 || k == n-1) ? '\n' : ' ');
			}else{
				int n0;
				strip0 = xTriStrip(m->numTris, m->tris, 0, &st0, &n0);
				ok &= xTriStripVerify(m->numTris, m->tris, n0, strip0);
				allok &= ok;
				free(strip0);
				printf("%-24s %6d %6d %7d %7d %6d %7d %6d %7d %6.3f %6.2f%s\n", name,
					m->numTris, m->welded, 3*m->numTris, n0, st0.numStrips, n, st.numStrips,
					st.numTunnels, (double)n/m->numTris, t1-t0, ok ? "" : "  VERIFY FAILED");
			}
			if(verbose && !ok)
				printf("  %s: %d indices\n", name, n);
			free(strip);
			free(m->tris);
			free(m->verts);
		}
		free(ms);
	}
	if(of)
		fclose(of);
	return allok ? 0 : 1;
}
