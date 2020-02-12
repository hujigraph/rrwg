#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "rrwg.h"

#define TITLE "Random Repelling Walks on Graphs"
#define XLAB "time"
#define YLAB "visits"

static char buf[BUFLEN]; /* local buffer */
#define FPRINTF(...) do {				\
		snprintf(&buf[0], BUFLEN, __VA_ARGS__);	\
		fprintf(file, "%s", buf);		\
	} while(0)

#define NSTYLES 3
static char*colors[NSTYLES]= {"red", "black", "dark blue"};

static char filename[MAXFN]; /* Output file name where the commands to plot are written. */
static FILE *file; /* File to write the commands to plot the curves. */

static struct {
	int x;
	double y;
} max;

static void assign_max(struct graph *g) {
	max.x = g->maxsteps;
	max.y = 1.0;
}

static void R_load_data(struct graph *g) {
	FPRINTF("data <- read.table(\"%s%s\", header=FALSE, sep=\"%s\")\n",
		g->name, ".dat", DATA_SEP);
	FPRINTF("xmax <- nrow(data) # xmax is the number of rows\n");
        FPRINTF("par(mfrow=c(%d, 1)) # set the number of rows\n",
		g->w);
}

static void R_plot(struct graph *g, int walker_idx) {
        FPRINTF("ymin <- min(data[,%d:%d])\n", (walker_idx*g->n)+1, (walker_idx+1)*g->n);
        FPRINTF("ymax <- max(data[,%d:%d])\n", (walker_idx*g->n)+1, (walker_idx+1)*g->n);
	FPRINTF("plot(data[,%d], col=\"%s\", type=\"l\", lwd=2, xlab=\"%s\", ylab=\"%s\", \
                       xlim=c(0,%d), ylim=c(ymin,ymax))\n",
		(walker_idx * g->n) + 1, colors[0], "n", "%visits", g->maxsteps);
}

static void R_line(struct graph *g, int particle_idx, int vertex_idx) {
	FPRINTF ("lines(data[,%d], col=\"%s\", lwd=2)\n",
		 (particle_idx * g->n) + (vertex_idx + 1), colors[vertex_idx % NSTYLES]);

}

/*
  Fill buf[] array with the command to set plot title, xlabel and
  ylabel in R. The function used for this task is as follows:

  title(main="main title", sub="sub-title",
  xlab="x-axis label", ylab="y-axis label")
*/
static void R_legend(struct graph *g) {
        int i;

        FPRINTF("legend(xmax-xmax/10, ymax-ymax/10, legend=c(");
        for (i=0; i<g->n; i++) {
                FPRINTF("\"v%d\"", i+1);
                if (i != g->n-1)
                        FPRINTF(",");
        }

	FPRINTF("), col=c(");
        for (i=0; i<g->n; i++) {
                FPRINTF("\"%s\"", colors[i%NSTYLES]);
                if (i != g->n-1)
                        FPRINTF(",");
        }
        FPRINTF("), ncol=1, cex=0.8, lwd=2, bty=\"n\")\n");
}

/*
  Fill the buffer with commands to finish the plot.
*/
static void R_end() {
	FPRINTF(buf, BUFLEN, "dev.off() # Close the file\n");
}

void R_fwrite_script(struct graph *g) {
	int i, j;

	strncpy(filename, g->name, MAXFN);
	append_suffix(filename, ".R");
	file = Fopen(filename, "w");

	assign_max(g);

	R_load_data(g);
	for (i = 0; i < g->w; i++) {
		R_plot(g, i);
		for (j = 1; j < g->n; j++)
			R_line(g, i, j);
                R_legend(g);
	}
	R_end();

	Fclose(file);
	fprintf(stderr, "* Wrote %s\n", filename);
}