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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resample.h"

mad_fixed_t(*Resampled)[2][MAX_NSAMPLES];
struct resample_state Resample;

int resample_init(struct resample_state *state, unsigned int oldrate,
				  unsigned int newrate)
{
	mad_fixed_t ratio;

	if (newrate == 0)
		return -1;

	ratio = mad_f_div(oldrate, newrate);
	if (ratio <= 0 || ratio > MAX_RESAMPLEFACTOR * MAD_F_ONE)
		return -1;

	state->ratio = ratio;

	state->step = 0;
	state->last = 0;

	return 0;
}

unsigned int resample_block(struct resample_state *state,
							unsigned int nsamples,
							mad_fixed_t const *old0,
							mad_fixed_t const *old1,
							mad_fixed_t * new0, mad_fixed_t * new1)
{
	mad_fixed_t const *end, *begin;

	if (state->ratio == MAD_F_ONE) {
		memcpy(new0, old0, nsamples * sizeof(mad_fixed_t));
		memcpy(new1, old1, nsamples * sizeof(mad_fixed_t));
		return nsamples;
	}

	end = old0 + nsamples;
	begin = new0;

	if (state->step < 0) {
		state->step = mad_f_fracpart(-state->step);

		while (state->step < MAD_F_ONE) {
			if (state->step) {
				*new0++ =
					state->last + mad_f_mul(*old0 - state->last, state->step);
				*new1++ =
					state->last + mad_f_mul(*old1 - state->last, state->step);
			} else {
				*new0++ = state->last;
				*new1++ = state->last;
			}

			state->step += state->ratio;
			if (((state->step + 0x00000080L) & 0x0fffff00L) == 0)
				state->step = (state->step + 0x00000080L) & ~0x0fffffffL;
		}

		state->step -= MAD_F_ONE;
	}

	while (end - old0 > 1 + mad_f_intpart(state->step)) {
		old0 += mad_f_intpart(state->step);
		old1 += mad_f_intpart(state->step);
		state->step = mad_f_fracpart(state->step);

		if (state->step) {
			*new0++ = *old0 + mad_f_mul(old0[1] - old0[0], state->step);
			*new1++ = *old1 + mad_f_mul(old1[1] - old1[0], state->step);
		} else {
			*new0++ = *old0;
			*new1++ = *old1;
		}

		state->step += state->ratio;
		if (((state->step + 0x00000080L) & 0x0fffff00L) == 0)
			state->step = (state->step + 0x00000080L) & ~0x0fffffffL;
	}

	if (end - old0 == 1 + mad_f_intpart(state->step)) {
		state->last = end[-1];
		state->step = -state->step;
	} else
		state->step -= mad_f_fromint(end - old0);

	return new0 - begin;
}
