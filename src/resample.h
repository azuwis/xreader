#include "mad.h"

// resample from madplay, thanks to nj-zero
#define MAX_RESAMPLEFACTOR 6
#define MAX_NSAMPLES (1152 * MAX_RESAMPLEFACTOR)

struct resample_state
{
	mad_fixed_t ratio;
	mad_fixed_t step;
	mad_fixed_t last;
};

extern mad_fixed_t(*Resampled)[2][MAX_NSAMPLES];
extern struct resample_state Resample;

int resample_init(struct resample_state *state, unsigned int oldrate,
						   unsigned int newrate);
unsigned int resample_block(struct resample_state *state,
									 unsigned int nsamples,
									 mad_fixed_t const *old0,
									 mad_fixed_t const *old1,
									 mad_fixed_t * new0, mad_fixed_t * new1);


