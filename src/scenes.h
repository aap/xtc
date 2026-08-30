/* The example/test scenes; include after xtc.h.
 *
 * The skeleton establishes a baseline before every draw call - depth test
 * on, blend/fog/texture/clipping off, linear filter, RGB modulate, world
 * matrix identity - so a scene only sets what it wants and cleans up
 * nothing. */

STRUCT(Scene) {
	const char *name;
	void (*draw)(void);
};

extern Scene scenes[];
extern int numScenes;

/* load the textures; call once the library is up */
void scenesInit(void);
