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

#ifndef __CL_TYPES_H__
#define __CL_TYPES_H__

#include "renderer/r_types.h"
#include "sound/s_types.h"
#include "ui/ui_types.h"

typedef struct cl_frame_s {
	bool valid; // cleared if delta parsing was invalid
	unsigned int server_frame;
	unsigned int server_time; // server time the message is valid for (in milliseconds)
	int delta_frame; // negatives indicate no delta
	byte area_bits[MAX_BSP_AREAS >> 3]; // portal area visibility bits
	player_state_t ps;
	unsigned short num_entities;
	unsigned int entity_state; // non-masked index into cl.entity_states array
} cl_frame_t;

typedef struct cl_entity_animation_s {
	entity_animation_t animation;
	unsigned int time;
	unsigned short frame;
	unsigned short old_frame;
	float lerp;
	float fraction;
} cl_entity_animation_t;

typedef struct cl_entity_s {
	entity_state_t baseline; // delta from this if not from a previous frame
	entity_state_t current;
	entity_state_t prev; // will always be valid, but might just be a copy of current

	unsigned int server_frame; // if not current, this entity isn't in the frame

	unsigned int time; // for intermittent effects

	cl_entity_animation_t animation1;
	cl_entity_animation_t animation2;

	r_lighting_t lighting; // cached static lighting info
} cl_entity_t;

typedef struct cl_client_info_s {
	char info[MAX_QPATH]; // the full info string, e.g. newbie\qforcer/blue
	char name[MAX_QPATH]; // the player name, e.g. newbie
	char model[MAX_QPATH]; // the model name, e.g. qforcer
	char skin[MAX_QPATH]; // the skin name, e.g. blue

	r_model_t *head;
	r_image_t *head_skins[MD3_MAX_MESHES];

	r_model_t *upper;
	r_image_t *upper_skins[MD3_MAX_MESHES];

	r_model_t *lower;
	r_image_t *lower_skins[MD3_MAX_MESHES];
} cl_client_info_t;

#define CMD_BACKUP 128  // allow a lot of command backups for very fast systems
#define CMD_MASK (CMD_BACKUP - 1)

// we accumulate parsed entity states in a rather large buffer so that they
// may be safely delta'd in the future
#define ENTITY_STATE_BACKUP (UPDATE_BACKUP * MAX_PACKET_ENTITIES)
#define ENTITY_STATE_MASK (ENTITY_STATE_BACKUP - 1)

// the cl_client_s structure is wiped completely at every map change
typedef struct cl_client_s {
	unsigned int time_demo_frames;
	unsigned int time_demo_start;

	unsigned int frame_counter;
	unsigned int packet_counter;
	unsigned int byte_counter;

	user_cmd_t cmds[CMD_BACKUP]; // each message will send several old cmds
	unsigned int cmd_time[CMD_BACKUP]; // time sent, for calculating pings

	vec_t predicted_step;
	unsigned int predicted_step_time;

	vec3_t predicted_origin; // generated by Cl_PredictMovement
	vec3_t predicted_offset;
	vec3_t predicted_angles;
	vec3_t prediction_error;
	struct g_edict_s *predicted_ground_entity;
	short predicted_origins[CMD_BACKUP][3]; // for debug comparing against server

	cl_frame_t frame; // received from server
	cl_frame_t frames[UPDATE_BACKUP]; // for calculating delta compression

	cl_entity_t entities[MAX_EDICTS]; // client entities

	entity_state_t entity_states[ENTITY_STATE_BACKUP]; // accumulated each frame
	unsigned int entity_state; // index (not wrapped) into entity states

	unsigned short player_num; // our entity number

	unsigned int surpress_count; // number of messages rate suppressed

	unsigned int time; // this is the server time value that the client
	// is rendering at.  always <= cls.real_time due to latency

	float lerp; // linear interpolation between frames

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta when necessary which is added to the locally
	// tracked view angles to account for spawn and teleport direction changes
	vec3_t angles;

	unsigned int server_count; // server identification for precache
	unsigned short server_frame_rate; // server frame rate (packets per second)

	bool demo_server; // we're viewing a demo
	bool third_person; // we're using a 3rd person camera

	char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

	// locally derived information from server state
	r_model_t *model_draw[MAX_MODELS];
	c_model_t *model_clip[MAX_MODELS];

	s_sample_t *sound_precache[MAX_SOUNDS];
	r_image_t *image_precache[MAX_IMAGES];

	cl_client_info_t client_info[MAX_CLIENTS];
} cl_client_t;

// the client_static_t structure is persistent through an arbitrary
// number of server connections

typedef enum {
	CL_UNINITIALIZED, CL_DISCONNECTED, // not talking to a server
	CL_CONNECTING, // sending request packets to the server
	CL_CONNECTED, // netchan_t established, waiting for svc_server_data
	CL_ACTIVE
// game views should be displayed
} cl_state_t;

typedef enum {
	KEY_GAME, KEY_UI, KEY_CONSOLE, KEY_CHAT
} cl_key_dest_t;

#define KEY_HISTORYSIZE 64
#define KEY_LINESIZE 256

typedef enum {
	K_FIRST,

	K_CTRL_A = 1,
	K_CTRL_E = 5,

	K_BACKSPACE = 8,
	K_TAB = 9,
	K_ENTER = 13,
	K_PAUSE = 19,
	K_ESCAPE = 27,
	K_SPACE = 32,
	K_DEL = 127,

	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MWHEELDOWN,
	K_MWHEELUP,
	K_MOUSE4,
	K_MOUSE5,

	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,

	K_NUMLOCK,

	K_KP_INS,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_ENTER,

	K_UPARROW,
	K_DOWNARROW,
	K_RIGHTARROW,
	K_LEFTARROW,

	K_HOME,
	K_END,
	K_PGUP,
	K_PGDN,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_INS,
	K_SHIFT,
	K_CTRL,
	K_ALT,

	K_LAST = 511
// to support as many chars as possible
} key_num_t;

typedef struct cl_key_state_s {
	cl_key_dest_t dest;

	char lines[KEY_HISTORYSIZE][KEY_LINESIZE];
	unsigned short pos;

	bool insert;
	bool repeat;

	unsigned int edit_line;
	unsigned int history_line;

	char *binds[K_LAST];
	bool down[K_LAST];
} cl_key_state_t;

typedef struct cl_mouse_state_s {
	float x, y;
	float old_x, old_y;
	bool grabbed;
} cl_mouse_state_t;

typedef struct cl_chat_state_s {
	char buffer[KEY_LINESIZE];
	size_t len;
	bool team;
} cl_chat_state_t;

typedef struct cl_download_s {
	bool http;
	FILE *file;
	char tempname[MAX_OSPATH];
	char name[MAX_OSPATH];
} cl_download_t;

typedef enum {
	SERVER_SOURCE_INTERNET, SERVER_SOURCE_USER, SERVER_SOURCE_BCAST
} cl_server_source_t;

typedef struct cl_server_info_s {
	net_addr_t addr;
	cl_server_source_t source;
	char hostname[64];
	char name[32];
	char gameplay[32];
	unsigned short clients;
	unsigned short max_clients;
	unsigned int ping_time;
	unsigned short ping;
	struct cl_server_info_s *next;
} cl_server_info_t;

#define MAX_SERVER_INFOS 128

typedef struct cl_static_s {
	cl_state_t state;

	cl_key_state_t key_state;

	cl_mouse_state_t mouse_state;

	cl_chat_state_t chat_state;

	unsigned int real_time; // always increasing, no clamping, etc

	unsigned int packet_delta; // milliseconds since last outgoing packet
	unsigned int render_delta; // milliseconds since last renderer frame

	// connection information
	char server_name[MAX_OSPATH]; // name of server to connect to
	unsigned int connect_time; // for connection retransmits

	net_chan_t netchan; // network channel

	unsigned int challenge; // from the server to use for connecting
	unsigned int spawn_count;

	unsigned short loading; // loading percentage indicator

	char download_url[MAX_OSPATH]; // for http downloads
	cl_download_t download; // current download (udp or http)

	char demo_path[MAX_OSPATH];
	FILE *demo_file;

	cl_server_info_t *servers; // list of servers from all sources
	char *servers_text; // tabular data for servers menu

	unsigned int broadcast_time; // time when last broadcast ping was sent

	struct cg_export_s *cgame;
} cl_static_t;

#endif /* __CL_TYPES_H__ */
