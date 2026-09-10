/*
 * demos/fox -- the fox, idling.  Everything of it is in fox.c, which is
 * its own program (main calls skelInit, foxInit and skelRun(foxFrame));
 * these two are what another program would need to embed it.
 */

#ifndef FOX_H
#define FOX_H

/* load the model and the animation.  Does nothing the second time, and
 * foxFrame calls it, so a caller only needs it to pay the load early. */
void foxInit(void);

/* one frame: the pad, the state machine, the lights, the fox */
void foxFrame(void);

#endif
