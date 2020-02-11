#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "rrwg.h"

/* Symbol to indicate the vertex where the particle starts at. */
#define LOCSYM '@'

enum tokcat {ASSIGN, EDGE_SEP, SPACE};

char *filename; /* current file name */

static enum context ctx;

static FILE *fp; /* current file pointer */
static char buffer[BUFLEN]; /* general-purpose buffer */
static char *cp, *rcp; /* current character and replica of current character */
static int lineno; /* line number of current line */

/* buffers for key/value pairs */
static char key[MAXTOKEN];
static char val[MAXTOKEN];

/* Mark if all parameters were set. */
static short parms_ok = 0;

static inline void skip_spaces(void) {
	while(isspace(*cp)) cp++;
}

static inline void reinit_cp(void) {
	cp = &buffer[0];
}

static inline void reinit_keyval(void) {
	bzero(key, MAXTOKEN);
	bzero(val, MAXTOKEN);
}

static inline void reinit_vars(void) {
	reinit_cp();
	reinit_keyval();
}

static int is_comment(void) {
	reinit_cp();
	skip_spaces();
	if (*cp=='#') {
		return 1;
	}
	return 0;
}

static int input_ln(void) {
	reinit_cp();
	reinit_keyval(); /* put only to facilitate debug */
	while(1) {
		*cp = fgetc(fp);
		if (*cp=='\n'){
			*cp = '\0';
			break;
		}
		if (*cp==EOF) return 1;
		cp++;
	}
	return 0;
}

static INLINE void context_switch(struct graph*g, char*fn) {
	struct vertex *v;
        struct walker *w;
	char buf[MAXFN];
	int i;

/* Check some constraints or set properties before changing context */
	switch(ctx) {
	case GRAPH:
		/* Check if all parameters were checked. */
		if (!parms_ok) {
			fprintf(stderr, "%s:%d Not all parameters were set.\n",
				      fn, lineno);
                        exit(EXIT_FAILURE);
                }
		strncpy(buf, fn, MAXFN);
		/* Remove the extension of file name. */
		rm_ext(buf);
		/* Set the name of the graph with input file name
		 * without extension. */
		graph_set_name(buf);
		break;
	case VERTEX:
		/*
		  Check if all walkers have a initial location determined.
		*/
		for (i=0; i<g->w; i++) {
			w = &g->walkers[i];
                        v = w->path[0];
			if (v == NULL) {
				fprintf(stderr, "%s:%d Walker %d was not initialized.\n",
				      fn, lineno, (i+1));
                                exit(EXIT_FAILURE);
			}
		}
  		break;
	default:
		break;
	}
	ctx++;
}

static INLINE int cmp0(int c) {
	return c==(int)'=';
}

static INLINE int cmp1(int c) {
	return c==(int)'-';
}

static void fill_buffer_up_to(char buf[], enum tokcat stop, char*fn) {
	int (*cmp)(int c);

	switch (stop) {
	case ASSIGN:
		cmp=&cmp0;
		break;
	case SPACE:
		cmp=&isspace;
		break;
		/* TODO: improve fatal message */

	case EDGE_SEP:
		cmp=&cmp1;
	default:
		fprintf(stderr, "%s:%d Unknown token..", fn, lineno);
                exit(EXIT_FAILURE);
		break;
	}

	rcp = &buf[0];
	while(1) {
		if (*cp=='\0')/* the string is not keyval type */
			return;

		if (cmp((int)*cp)) {
			break;
		}
		*rcp++ = *cp++;
	}
	cp++, *rcp = '\0'; /* skip stop token and close key */
}

/*
  Make touch[] contains the key string
  and
  cp points to value string start.
*/
static void fill_keyval(char*fn) {
	if (*cp=='\0') return;

	skip_spaces(); /* touch key */
	fill_buffer_up_to(key, ASSIGN, fn); /* fill token stopping one char after '=' */
	skip_spaces(); /* touch value */
	fill_buffer_up_to(val, SPACE, fn); /* fill token stopping one char after SPACE cat */
}

static void scan_graph_keyval(struct graph*g, char *fn) {
	static char funcname[8];
	static double alpha=EMPTY_COEFF;
	static int maxsteps = NOSTEP;
	static int nwalkers=0;
	/* Count the number of parameters already set. */
	static int nparms_count=0;

	fill_keyval(fn);

	if (strncmp(key, "alpha", MAXTOKEN)==0) {
		alpha = atof(val);
		nparms_count++;
	} else if (strncmp(key, "function", MAXTOKEN)==0) {
		strncpy(funcname,  val, 8);
		nparms_count++;
	} else if (strncmp(key, "maxsteps", MAXTOKEN)==0) {
		maxsteps = atoi(val);
		nparms_count++;
	} else if (strncmp(key, "particles", MAXTOKEN)==0) {
		nwalkers = atoi(val);
		nparms_count++;
	} else {
		fprintf(stderr, "%s:%d Syntax error\n", fn, lineno);
                exit(EXIT_FAILURE);
        }

	/* All parameters were parsed. */
	if (nparms_count == NPARMS) {
		graph_init_walkers(g, nwalkers);
		g->alpha = alpha;
	        g->maxsteps =  maxsteps;
		graph_assign_function(g, funcname, fn);
		parms_ok = 1;
	}
}

static void scan_vertex_keyval(struct graph*g, struct vertex*v, char*fn) {
	int is_starting_point = 0; /* particle starts at the vertex? */
	int pidx=-1; /* particle index */
	struct walker *w;
	/*
	  a copy of key with '*' char removed
	  valgrind considers an error copying
	  from a var[] to the same var[] with
	  the offset of one char => &var[1].
	*/
	static char rkey[MAXTOKEN-1];

	fill_keyval(fn);

	/* Check if the vertex is a starting point of particle */
	if (key[0]==LOCSYM) {
		is_starting_point = 1;
		strncpy(rkey, &key[1], MAXTOKEN-1);
	} else
		strncpy(rkey, &key[0], MAXTOKEN-1);

	pidx = atoi(rkey) - 1;
	w = &g->walkers[pidx];
	w->steps0 = atoi(val);
	graph_visits_set(g, v, w, 0, w->steps0);
	if (is_starting_point)
			w->path[0] = v;
}

static void scan_vertex(struct graph *g, char*fn) {
	struct vertex *v;

	fill_buffer_up_to(key, SPACE, fn);
	v = graph_vertex_add(g, key);

	while (*cp) {
		skip_spaces();
		scan_vertex_keyval(g, v, fn);
	}
}

static void scan_edge(struct graph *g, char*fn) {
	fill_buffer_up_to(key, EDGE, fn); /* from vertex */
	cp += 2; /* skip '--' on "a -- b" edge*/
	skip_spaces();
	fill_buffer_up_to(val, SPACE, fn); /* to vertex */
	graph_edge_add(g, key, val);
}

static void eval(struct graph *g, char*fn) {
	switch(ctx) {
	case GRAPH:
		scan_graph_keyval(g, fn);
		break;
	case VERTEX:
		scan_vertex(g, fn);
		break;

	case EDGE:
		scan_edge(g, fn);
		break;

	default:
		break;
	}
}

struct graph *pjk_read(struct graph *g, char *filename) {
	int eof = 0;

	fp = Fopen(filename, "r");
	while (1) {
		reinit_vars();
		printf("%s\n", buffer);

		lineno++;
		eof = input_ln();
		if (eof) break;

		if (is_comment() || *cp=='\0')
			continue;

		if (*cp=='*') {
			// TODO: get which context is in
			context_switch(g, filename);
			continue;
		}
		eval(g, filename);
	}
	Fclose(fp);

	/* a particle may stay at the same place */
	graph_self_loops_add(g);

	return g;
}

void rm_ext(char *fn) {
	assert(fn);

	cp = &fn[0];
	while (*cp) cp++;

	do {
		*cp-- = '\0';
	} while (*cp!='.');
	*cp = '\0'; /* overwrite '.' char */
}

void append_suffix(char str[], char *suf) {
	cp = &str[0];
	while (*cp) cp++;

	rcp = &suf[0];
	while (*rcp) {
		*cp++ = *rcp++;
		if (rcp-&str[0] >= MAXFN ) {
			fprintf(stderr, "%s:%d Exceed buffer capacity at \"%s\".\n",
			      __FILE__, (int)__LINE__, __FUNCTION__);
                        exit(errno);
                }
	}
}
