/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#include "sv_local.h"

/*
 * @brief Abort the server with a game error, always emitting ERR_DROP.
 */
static void Sv_GameError(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
static void Sv_GameError(const char *func, const char *fmt, ...) {
	char msg[MAX_STRING_CHARS];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	Com_Error(ERR_DROP, "!Game error: %s\n", msg);
}

/*
 * @brief Also sets mins and maxs for inline bsp models.
 */
static void Sv_SetModel(g_entity_t *ent, const char *name) {

	if (!name) {
		Com_Warn("%d: NULL\n", (int32_t) NUM_FOR_ENTITY(ent));
		return;
	}

	ent->s.model1 = Sv_ModelIndex(name);

	// if it is an inline model, get the size information for it
	if (name[0] == '*') {
		const cm_bsp_model_t *mod = Cm_Model(name);
		VectorCopy(mod->mins, ent->mins);
		VectorCopy(mod->maxs, ent->maxs);
		Sv_LinkEntity(ent);
	}
}

/*
 * @brief
 */
static void Sv_ConfigString(const uint16_t index, const char *val) {

	if (index >= MAX_CONFIG_STRINGS) {
		Com_Warn("Bad index %u\n", index);
		return;
	}

	if (!val)
		val = "";

	// make sure it's actually changed
	if (!g_strcmp0(sv.config_strings[index], val)) {
		return;
	}

	// change the string in sv.config_strings
	g_strlcpy(sv.config_strings[index], val, sizeof(sv.config_strings[0]));

	if (sv.state != SV_LOADING) { // send the update to everyone
		Mem_ClearBuffer(&sv.multicast);
		Net_WriteByte(&sv.multicast, SV_CMD_CONFIG_STRING);
		Net_WriteShort(&sv.multicast, index);
		Net_WriteString(&sv.multicast, val);

		Sv_Multicast(vec3_origin, MULTICAST_ALL_R);
	}
}

/*
 * Message wrappers which target the multicast buffer.
 */

static void Sv_WriteData(const void *data, size_t len) {
	Net_WriteData(&sv.multicast, data, len);
}

static void Sv_WriteChar(const int32_t c) {
	Net_WriteChar(&sv.multicast, c);
}

static void Sv_WriteByte(const int32_t c) {
	Net_WriteByte(&sv.multicast, c);
}

static void Sv_WriteShort(const int32_t c) {
	Net_WriteShort(&sv.multicast, c);
}

static void Sv_WriteLong(const int32_t c) {
	Net_WriteLong(&sv.multicast, c);
}

static void Sv_WriteString(const char *s) {
	Net_WriteString(&sv.multicast, s);
}

static void Sv_WriteVector(const vec_t v) {
	Net_WriteVector(&sv.multicast, v);
}

static void Sv_WritePosition(const vec3_t pos) {
	Net_WritePosition(&sv.multicast, pos);
}

static void Sv_WriteDir(const vec3_t dir) {
	Net_WriteDir(&sv.multicast, dir);
}

static void Sv_WriteAngle(const vec_t v) {
	Net_WriteAngle(&sv.multicast, v);
}

static void Sv_WriteAngles(const vec3_t angles) {
	Net_WriteAngles(&sv.multicast, angles);
}

/*
 * @brief Also checks areas so that doors block sight.
 */
static _Bool Sv_InPVS(const vec3_t p1, const vec3_t p2) {
	byte pvs[MAX_BSP_LEAFS >> 3];

	const int32_t leaf1 = Cm_PointLeafnum(p1, 0);
	const int32_t leaf2 = Cm_PointLeafnum(p2, 0);

	const int32_t area1 = Cm_LeafArea(leaf1);
	const int32_t area2 = Cm_LeafArea(leaf2);

	if (!Cm_AreasConnected(area1, area2))
		return false; // a door blocks sight

	const int32_t cluster1 = Cm_LeafCluster(leaf1);
	const int32_t cluster2 = Cm_LeafCluster(leaf2);

	Cm_ClusterPVS(cluster1, pvs);

	if ((pvs[cluster2 >> 3] & (1 << (cluster2 & 7))) == 0)
		return false;

	return true;
}

/*
 * @brief Also checks areas so that doors block sound.
 */
static _Bool Sv_InPHS(const vec3_t p1, const vec3_t p2) {
	byte phs[MAX_BSP_LEAFS >> 3];

	const int32_t leaf1 = Cm_PointLeafnum(p1, 0);

	const int32_t leaf2 = Cm_PointLeafnum(p2, 0);

	const int32_t area1 = Cm_LeafArea(leaf1);
	const int32_t area2 = Cm_LeafArea(leaf2);

	if (!Cm_AreasConnected(area1, area2))
		return false; // a door blocks hearing

	const int32_t cluster1 = Cm_LeafCluster(leaf1);
	const int32_t cluster2 = Cm_LeafCluster(leaf2);

	Cm_ClusterPHS(cluster1, phs);

	if ((phs[cluster2 >> 3] & (1 << (cluster2 & 7))) == 0)
		return false;

	return true;
}

/*
 * @brief
 */
static void Sv_Sound(const g_entity_t *ent, const uint16_t index, const uint16_t atten) {

	if (!ent)
		return;

	Sv_PositionedSound(NULL, ent, index, atten);
}

static void *game_handle;

/*
 * @brief Initializes the game module by exposing a subset of server functionality
 * through function pointers. In return, the game module allocates memory for
 * entities and returns a few pointers of its own.
 *
 * Note that the terminology here is worded from the game module's perspective;
 * that is, "import" is what we give to the game, and "export" is what the game
 * returns to us. This distinction seems a bit backwards, but it was likely
 * deemed less confusing to "mod" authors back in the day.
 */
void Sv_InitGame(void) {
	g_import_t import;

	if (svs.game) {
		Com_Error(ERR_FATAL, "Game already loaded");
	}

	Com_Print("Game initialization...\n");

	memset(&import, 0, sizeof(import));

	import.frame_rate = svs.frame_rate;
	import.frame_millis = 1000 / svs.frame_rate;
	import.frame_seconds = 1.0 / svs.frame_rate;

	import.Print = Com_Print;
	import.Debug_ = Com_Debug_;
	import.Warn_ = Com_Warn_;
	import.Error_ = Sv_GameError;

	import.Malloc = Mem_TagMalloc;
	import.LinkMalloc = Mem_LinkMalloc;
	import.Free = Mem_Free;
	import.FreeTag = Mem_FreeTag;

	import.LoadFile = Fs_Load;
	import.FreeFile = Fs_Free;

	import.Cvar = Cvar_Get;
	import.Cmd = Cmd_Add;
	import.Argc = Cmd_Argc;
	import.Argv = Cmd_Argv;
	import.Args = Cmd_Args;

	import.AddCommandString = Cbuf_AddText;

	import.ConfigString = Sv_ConfigString;

	import.ModelIndex = Sv_ModelIndex;
	import.SoundIndex = Sv_SoundIndex;
	import.ImageIndex = Sv_ImageIndex;

	import.SetModel = Sv_SetModel;
	import.Sound = Sv_Sound;
	import.PositionedSound = Sv_PositionedSound;

	import.Trace = Sv_Trace;
	import.PointContents = Sv_PointContents;
	import.inPVS = Sv_InPVS;
	import.inPHS = Sv_InPHS;
	import.SetAreaPortalState = Cm_SetAreaPortalState;
	import.AreasConnected = Cm_AreasConnected;

	import.LinkEntity = Sv_LinkEntity;
	import.UnlinkEntity = Sv_UnlinkEntity;
	import.BoxEntities = Sv_BoxEntities;

	import.Multicast = Sv_Multicast;
	import.Unicast = Sv_Unicast;
	import.WriteData = Sv_WriteData;
	import.WriteChar = Sv_WriteChar;
	import.WriteByte = Sv_WriteByte;
	import.WriteShort = Sv_WriteShort;
	import.WriteLong = Sv_WriteLong;
	import.WriteString = Sv_WriteString;
	import.WriteVector = Sv_WriteVector;
	import.WritePosition = Sv_WritePosition;
	import.WriteDir = Sv_WriteDir;
	import.WriteAngle = Sv_WriteAngle;
	import.WriteAngles = Sv_WriteAngles;

	import.BroadcastPrint = Sv_BroadcastPrint;
	import.ClientPrint = Sv_ClientPrint;

	svs.game = (g_export_t *) Sys_LoadLibrary("game", &game_handle, "G_LoadGame", &import);

	if (!svs.game) {
		Com_Error(ERR_DROP, "Failed to load game module\n");
	}

	if (svs.game->api_version != GAME_API_VERSION) {
		Com_Error(ERR_DROP, "Game is version %i, not %i\n", svs.game->api_version,
				GAME_API_VERSION);
	}

	svs.game->Init();

	Com_Print("Game initialized, starting...\n");
	Com_InitSubsystem(QUETOO_GAME);
}

/*
 * @brief Called when either the entire server is being killed, or it is changing to a
 * different game directory.
 */
void Sv_ShutdownGame(void) {

	if (!svs.game)
		return;

	Com_Print("Game shutdown...\n");

	svs.game->Shutdown();
	svs.game = NULL;

	Cmd_RemoveAll(CMD_GAME);

	// the game module code should call this, but lets not assume
	Mem_FreeTag(MEM_TAG_GAME_LEVEL);
	Mem_FreeTag(MEM_TAG_GAME);

	Com_Print("Game down\n");
	Com_QuitSubsystem(QUETOO_GAME);

	Sys_CloseLibrary(&game_handle);
}
