/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

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
