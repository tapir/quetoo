/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "client.h"

typedef struct loc_s {
	vec3_t loc;
	char desc[MAX_STRING_CHARS];
} loc_t;

#define MAX_LOCATIONS 1024

loc_t locations[MAX_LOCATIONS];
int numlocations;

/*
 * Cl_ClearLocations
 *
 * Effectively clears all locations for the current level.
 */
static void Cl_ClearLocations(void){
	numlocations = 0;
}

/*
 * Cl_LoadLocations
 *
 * Parse a .loc file for the current level.
 */
void Cl_LoadLocations(void){
	const char *c;
	char filename[MAX_QPATH];
	FILE *f;
	int i;

	Cl_ClearLocations();  // clear any resident locations
	i = 0;

	// load the locations file
	c = Com_Basename(cl.configstrings[CS_MODELS + 1]);
	snprintf(filename, sizeof(filename), "locations/%s", c);
	strcpy(filename + strlen(filename) - 3, "loc");

	if(Fs_OpenFile(filename, &f, FILE_READ) == -1){
		Com_Dprintf("Couldn't load %s\n", filename);
		return;
	}

	while(i < MAX_LOCATIONS){

		const int err = fscanf(f, "%f %f %f %[^\n]",
				&locations[i].loc[0], &locations[i].loc[1],
				&locations[i].loc[2], locations[i].desc
		);

		numlocations = i;
		if (err == EOF)
			break;
		i++;
	}

	Com_Printf("Loaded %i locations.\n", numlocations);
	Fs_CloseFile(f);
}


/*
 * Cl_SaveLocations_f
 *
 * Write locations for current level to file.
 */
static void Cl_SaveLocations_f(void){
	char filename[MAX_QPATH];
	FILE *f;
	int i;

	snprintf(filename, sizeof(filename), "%s/%s", Fs_Gamedir(), cl.configstrings[CS_MODELS + 1]);
	strcpy(filename + strlen(filename) - 3, "loc");  // change to .loc

	if((f = fopen(filename, "w")) == NULL){
		Com_Warn("Cl_SaveLocations_f: Failed to write %s\n", filename);
		return;
	}

	for(i = 0; i < numlocations; i++){
		fprintf(f, "%d %d %d %s\n",
				(int)locations[i].loc[0], (int)locations[i].loc[1],
				(int)locations[i].loc[2], locations[i].desc
		);
	}

	Com_Printf("Saved %d locations.\n", numlocations);
	Fs_CloseFile(f);
}


/*
 * Cl_Location
 *
 * Returns the description of the location nearest nearto.
 */
static const char *Cl_Location(const vec3_t nearto){
	vec_t dist, mindist;
	vec3_t v;
	int i, j;

	if(numlocations == 0)
		return "";

	mindist = 999999;

	for(i = 0, j = 0; i < numlocations; i++){  // find closest loc

		VectorSubtract(nearto, locations[i].loc, v);
		if((dist = VectorLength(v)) < mindist){  // closest yet
			mindist = dist;
			j = i;
		}
	}

	return locations[j].desc;
}


/*
 * Cl_LocationHere
 *
 * Returns the description of the location nearest the client.
 */
const char *Cl_LocationHere(void){
	return Cl_Location(r_view.origin);
}


/*
 * Cl_LocationThere
 *
 * Returns the description of the location nearest the client's crosshair.
 */
const char *Cl_LocationThere(void){
	vec3_t dest;

	// project vector from view position and angle
	VectorMA(r_view.origin, 8192.0, r_view.forward, dest);

	// and trace to world model
	R_Trace(r_view.origin, dest, 0, MASK_SHOT);

	return Cl_Location(r_view.trace.endpos);
}


/*
 * Cl_AddLocation
 *
 * Add a new location described by desc at nearto.
 */
static void Cl_AddLocation(const vec3_t nearto, const char *desc){

	if(numlocations >= MAX_LOCATIONS)
		return;

	VectorCopy(nearto, locations[numlocations].loc);
	strncpy(locations[numlocations].desc, desc, MAX_STRING_CHARS);

	numlocations++;
}


/*
 * Cl_AddLocation_f
 *
 * Command callback for adding locations in game.
 */
static void Cl_AddLocation_f(void){

	if(Cmd_Argc() < 2){
		Com_Printf("Usage: %s <description>\n", Cmd_Argv(0));
		return;
	}

	Cl_AddLocation(r_view.origin, Cmd_Args());
}


/*
 * Cl_InitLocations
 */
void Cl_InitLocations(void){
	Cmd_AddCommand("addloc", Cl_AddLocation_f, NULL);
	Cmd_AddCommand("savelocs", Cl_SaveLocations_f, NULL);
}


/*
 * Cl_ShutdownLocations
 */
void Cl_ShutdownLocations(void){
	Cmd_RemoveCommand("addloc");
	Cmd_RemoveCommand("savelocs");
}
