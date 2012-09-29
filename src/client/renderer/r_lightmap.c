/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/*
 * In video memory, lightmaps are chunked into NxN RGB blocks. In the bsp,
 * they are a contiguous lump. During the loading process, we use floating
 * point to provide precision.
 */

r_lightmaps_t r_lightmaps;

/*
 * @brief
 */
static void R_UploadLightmapBlock() {

	if (r_lightmaps.lightmap_texnum == MAX_GL_LIGHTMAPS) {
		Com_Warn("R_UploadLightmapBlock: MAX_GL_LIGHTMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.lightmap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.block_size, r_lightmaps.block_size, 0,
			GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	r_lightmaps.lightmap_texnum++;

	if (r_models.load->version == BSP_VERSION_) { // upload deluxe block as well

		if (r_lightmaps.deluxemap_texnum == MAX_GL_DELUXEMAPS) {
			Com_Warn("R_UploadLightmapBlock: MAX_GL_DELUXEMAPS reached.\n");
			return;
		}

		R_BindTexture(r_lightmaps.deluxemap_texnum);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.block_size, r_lightmaps.block_size, 0,
				GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.direction_buffer);

		r_lightmaps.deluxemap_texnum++;
	}

	// clear the allocation block and buffers
	memset(r_lightmaps.allocated, 0, r_lightmaps.block_size * 3);
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.block_size * r_lightmaps.block_size * 3);
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.block_size * r_lightmaps.block_size * 3);
}

/*
 * @brief
 */
static bool R_AllocLightmapBlock(r_pixel_t w, r_pixel_t h, r_pixel_t *x, r_pixel_t *y) {
	r_pixel_t i, j;
	r_pixel_t best;

	best = r_lightmaps.block_size;

	for (i = 0; i < r_lightmaps.block_size - w; i++) {
		r_pixel_t best2 = 0;

		for (j = 0; j < w; j++) {
			if (r_lightmaps.allocated[i + j] >= best)
				break;
			if (r_lightmaps.allocated[i + j] > best2)
				best2 = r_lightmaps.allocated[i + j];
		}
		if (j == w) { // this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > r_lightmaps.block_size)
		return false;

	for (i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

	return true;
}

/*
 * @brief
 */
static void R_BuildDefaultLightmap(r_bsp_surface_t *surf, byte *sout, byte *dout, size_t stride) {
	int32_t i, j;

	const r_pixel_t smax = (surf->st_extents[0] / r_models.load->bsp->lightmap_scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / r_models.load->bsp->lightmap_scale) + 1;

	stride -= (smax * 3);

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {

			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;

			sout += 3;

			if (r_models.load->version == BSP_VERSION_) {
				dout[0] = 127;
				dout[1] = 127;
				dout[2] = 255;

				dout += 3;
			}
		}
	}
}

/*
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGBA to the specified destinations.
 */
static void R_BuildLightmap(r_bsp_surface_t *surf, byte *sout, byte *dout, size_t stride) {
	size_t i, j;
	byte *lightmap, *lm, *deluxemap, *dm;

	const r_pixel_t smax = (surf->st_extents[0] / r_models.load->bsp->lightmap_scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / r_models.load->bsp->lightmap_scale) + 1;

	const size_t size = smax * tmax;
	stride -= (smax * 3);

	lightmap = (byte *) Z_Malloc(size * 3);
	lm = lightmap;

	deluxemap = dm = NULL;

	if (r_models.load->version == BSP_VERSION_) {
		deluxemap = (byte *) Z_Malloc(size * 3);
		dm = deluxemap;
	}

	// convert the raw lightmap samples to RGBA for softening
	for (i = j = 0; i < size; i++, lm += 3, dm += 3) {
		lm[0] = surf->samples[j++];
		lm[1] = surf->samples[j++];
		lm[2] = surf->samples[j++];

		// read in directional samples for deluxe mapping as well
		if (r_models.load->version == BSP_VERSION_) {
			dm[0] = surf->samples[j++];
			dm[1] = surf->samples[j++];
			dm[2] = surf->samples[j++];
		}
	}

	// apply modulate, contrast, resolve average surface color, etc..
	R_FilterTexture(lightmap, smax, tmax, NULL, it_lightmap);

	// soften it if it's sufficiently large
	if (r_soften->value && size > 128) {
		for (i = 0; i < r_soften->value; i++) {
			R_SoftenTexture(lightmap, smax, tmax, it_lightmap);

			if (r_models.load->version == BSP_VERSION_)
				R_SoftenTexture(deluxemap, smax, tmax, it_deluxemap);
		}
	}

	// the lightmap is uploaded to the card via the strided block

	lm = lightmap;

	if (r_models.load->version == BSP_VERSION_)
		dm = deluxemap;

	r_pixel_t s, t;
	for (t = 0; t < tmax; t++, sout += stride, dout += stride) {
		for (s = 0; s < smax; s++) {

			// copy the lightmap to the strided block
			sout[0] = lm[0];
			sout[1] = lm[1];
			sout[2] = lm[2];
			sout += 3;

			lm += 3;

			// and the deluxemap for maps which include it
			if (r_models.load->version == BSP_VERSION_) {
				dout[0] = dm[0];
				dout[1] = dm[1];
				dout[2] = dm[2];
				dout += 3;

				dm += 3;
			}
		}
	}

	Z_Free(lightmap);

	if (r_models.load->version == BSP_VERSION_)
		Z_Free(deluxemap);
}

/*
 * @brief
 */
void R_CreateSurfaceLightmap(r_bsp_surface_t *surf) {

	if (!(surf->flags & R_SURF_LIGHTMAP))
		return;

	const r_pixel_t smax = (surf->st_extents[0] / r_models.load->bsp->lightmap_scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / r_models.load->bsp->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {

		R_UploadLightmapBlock(); // upload the last block

		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
			Com_Error(ERR_DROP, "R_CreateSurfaceLightmap: Consecutive calls to "
				"R_AllocLightmapBlock(%d,%d) failed.", smax, tmax);
		}
	}

	surf->lightmap_texnum = r_lightmaps.lightmap_texnum;
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnum;

	byte *samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t * r_lightmaps.block_size + surf->light_s) * 3;

	byte *directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t * r_lightmaps.block_size + surf->light_s) * 3;

	const size_t stride = r_lightmaps.block_size * 3;

	if (surf->samples)
		R_BuildLightmap(surf, samples, directions, stride);
	else
		R_BuildDefaultLightmap(surf, samples, directions, stride);
}

/*
 * @brief
 */
void R_BeginBuildingLightmaps(void) {
	int32_t max;

	// users can tune lightmap size for their card
	r_lightmaps.block_size = r_lightmap_block_size->integer;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

	// but clamp it to the card's capability to avoid errors
	r_lightmaps.block_size = Clamp(r_lightmaps.block_size, 256, (r_pixel_t) max);

	const r_pixel_t bs = r_lightmaps.block_size;

	r_lightmaps.allocated = R_HunkAlloc(bs * sizeof(r_pixel_t));

	r_lightmaps.sample_buffer = R_HunkAlloc(bs * bs * sizeof(uint32_t));
	r_lightmaps.direction_buffer = R_HunkAlloc(bs * bs * sizeof(uint32_t));

	r_lightmaps.lightmap_texnum = TEXNUM_LIGHTMAPS;
	r_lightmaps.deluxemap_texnum = TEXNUM_DELUXEMAPS;
}

/*
 * @brief
 */
void R_EndBuildingLightmaps(void) {

	// upload the pending lightmap block
	R_UploadLightmapBlock();
}
