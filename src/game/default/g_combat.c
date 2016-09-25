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

#include "g_local.h"
#include "bg_pmove.h"

/**
 * @brief Returns true if ent1 and ent2 are on the same team.
 */
_Bool G_OnSameTeam(const g_entity_t *ent1, const g_entity_t *ent2) {

	if (!ent1->client || !ent2->client)
		return false;

	if (ent1->client->locals.persistent.spectator && ent2->client->locals.persistent.spectator)
		return true;

	if (!g_level.teams && !g_level.ctf)
		return false;

	return ent1->client->locals.persistent.team == ent2->client->locals.persistent.team;
}

/**
 * @brief Returns true if the inflictor can directly damage the target. Used for
 * explosions and melee attacks.
 */
_Bool G_CanDamage(g_entity_t *targ, g_entity_t *inflictor) {
	vec3_t dest;
	cm_trace_t tr;

	// BSP sub-models need special checking because their origin is 0,0,0
	if (targ->solid == SOLID_BSP) {
		VectorAdd(targ->abs_mins, targ->abs_maxs, dest);
		VectorScale(dest, 0.5, dest);
		tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
		if (tr.fraction == 1.0)
			return true;
		if (tr.ent == targ)
			return true;
		return false;
	}

	tr = gi.Trace(inflictor->s.origin, targ->s.origin, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	return false;
}

/**
 * @brief
 */
static void G_SpawnDamage(g_temp_entity_t type, const vec3_t pos, const vec3_t normal,
		int16_t damage) {

	if (damage < 1)
		return;

	int16_t count = Clamp(damage / 50, 1, 4);

	while (count--) {
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(type);
		gi.WritePosition(pos);
		gi.WriteDir(normal ? normal : vec3_origin);
		gi.Multicast(pos, MULTICAST_PVS, NULL);
	}
}

/**
 * @brief Absorbs damage with the strongest armor the specified client holds.
 *
 * @return The amount of damage absorbed, which is not necessarily the amount
 * of armor consumed.
 */
static int16_t G_CheckArmor(g_entity_t *ent, const vec3_t pos, const vec3_t normal, int16_t damage,
		uint32_t dflags) {

	if (dflags & DMG_NO_ARMOR)
		return 0;

	if (!ent->client)
		return 0;

	const g_item_t *armor = G_ClientArmor(ent);
	const g_armor_info_t *armor_info = G_ArmorInfo(armor); 

	if (!armor)
		return 0;

	const int16_t quantity = ent->client->locals.inventory[ITEM_INDEX(armor)];
	int16_t saved;

	
	if (dflags & DMG_ENERGY)
		saved = Clamp(damage * armor_info->energy_protection, 0, quantity);
	else
		saved = Clamp(damage * armor_info->normal_protection, 0, quantity);	

	ent->client->locals.inventory[ITEM_INDEX(armor)] -= saved;

	G_SpawnDamage(TE_BLOOD, pos, normal, saved);

	return saved;
}

#define QUAD_DAMAGE_FACTOR 2.5
#define QUAD_KNOCKBACK_FACTOR 2.0

/**
 * @brief Damage routine. The inflictor imparts damage on the target on behalf
 * of the attacker.
 *
 * @param target The target may receive damage.
 * @param inflictor The entity inflicting the damage (projectile, optional).
 * @param attacker The entity taking credit for the damage (client, optional).
 * @param dir The direction of the attack (optional).
 * @param pos The point at which damage is being inflicted (optional).
 * @param normal The normal vector from that point (optional).
 * @param damage The damage to be inflicted.
 * @param knockback Velocity added to target in the direction of the normal.
 * @param dflags Damage flags:
 *
 *  DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
 * 	DAMAGE_NO_ARMOR			armor does not protect from this damage
 * 	DAMAGE_ENERGY			damage is from an energy based weapon
 * 	DAMAGE_BULLET			damage is from a bullet
 * 	DAMAGE_NO_PROTECTION	kills god mode, armor, everything
 *
 * @param mod The means of death, used by the obituaries routine.
 */
void G_Damage(g_entity_t *target, g_entity_t *inflictor, g_entity_t *attacker, const vec3_t dir,
		const vec3_t pos, const vec3_t normal, int16_t damage, int16_t knockback, uint32_t dflags,
		uint32_t mod) {

	if (!target || !target->locals.take_damage)
		return;

	if (target->client) { // respawn protection
		if (target->client->locals.respawn_protection_time > g_level.time)
			return;
	}

	inflictor = inflictor ? inflictor : g_game.entities;
	attacker = attacker ? attacker : g_game.entities;

	dir = dir ? dir : vec3_origin;
	pos = pos ? pos : target->s.origin;
	normal = normal ? normal : vec3_origin;

	if (attacker->client) {
		if (attacker->client->locals.inventory[g_media.items.quad_damage]) {
			damage *= QUAD_DAMAGE_FACTOR;
			knockback *= QUAD_KNOCKBACK_FACTOR;
		}

		damage *= attacker->client->locals.persistent.handicap / 100.0;
	}

	// friendly fire avoidance
	if (target != attacker && (g_level.teams || g_level.ctf)) {
		if (G_OnSameTeam(target, attacker)) { // target and attacker are on same team

			if (mod == MOD_TELEFRAG) { // telefrags can not be avoided
				mod |= MOD_FRIENDLY_FIRE;
			} else { // while everything else can
				if (g_friendly_fire->value)
					mod |= MOD_FRIENDLY_FIRE;
				else
					damage = 0;
			}
		}
	}

	// there is no self damage in instagib or arena, but there is knockback
	if (target == attacker) {
		switch (g_level.gameplay) {
			case GAME_INSTAGIB:
			case GAME_ARENA:
				damage = 0;
				break;
			default:
				break;
		}
	}

	g_client_t *client = target->client;

	// calculate velocity change due to knockback
	if (knockback && (target->locals.move_type >= MOVE_TYPE_WALK)) {
		vec3_t ndir, knockback_vel, knockback_avel;

		VectorCopy(dir, ndir);
		VectorNormalize(ndir);

		// knock the target upwards at least a bit; it's fun
		if (ndir[2] >= -0.25) {
			ndir[2] = MAX(0.25, ndir[2]);
			VectorNormalize(ndir);
		}

		// ensure the target has valid mass for knockback calculation
		const vec_t mass = Clamp(target->locals.mass, 1.0, 1000.0);

		// rocket jump hack
		const vec_t scale = (target == attacker ? 1200.0 : 800.0);

		VectorScale(ndir, scale * knockback / mass, knockback_vel);
		VectorAdd(target->locals.velocity, knockback_vel, target->locals.velocity);

		// apply angular velocity (rotate)
		if (client == NULL || (client->ps.pm_state.flags & PMF_GIBLET)) {
			VectorSet(knockback_avel, knockback, knockback, knockback);
			const vec_t ascale = 100.0 / mass;

			VectorMA(target->locals.avelocity, ascale, knockback_avel, target->locals.avelocity);
		}

		if (client) { // make sure the client can leave the ground
			client->ps.pm_state.flags |= PMF_TIME_PUSHED;
			client->ps.pm_state.time = 120;
		}
	}

	int16_t damage_armor = 0, damage_health = 0;

	// check for god mode protection
	if ((target->locals.flags & FL_GOD_MODE) && !(dflags & DMG_NO_GOD)) {
		damage_armor = damage;
		damage_health = 0;
		G_SpawnDamage(TE_BLOOD, pos, normal, damage);
	} else { // or armor protection
		damage_armor = G_CheckArmor(target, pos, normal, damage, dflags);
		damage_health = damage - damage_armor;
	}

	const _Bool was_dead = target->locals.dead;

	// do the damage
	if (damage_health && (target->locals.health || target->locals.dead)) {
		if (G_IsStructural(target, NULL)) { // impact things we can hurt but don't bleed
			if (dflags & DMG_BULLET)
				G_SpawnDamage(TE_BULLET, pos, normal, damage_health);
			else
				G_SpawnDamage(TE_SPARKS, pos, normal, damage_health);
		} else if (G_IsMeat(target)) { // bleed for everything else
			G_SpawnDamage(TE_BLOOD, pos, normal, damage_health);
		}

		target->locals.health -= damage_health;

		if (target->locals.health <= 0) {
			target->locals.dead = true;

			if (target->locals.Die) {
				target->locals.Die(target, attacker, mod);
			} else {
				gi.Debug("No die function for %s\n", target->class_name);
			}
			return;
		}
	}

	// if the target was already dead, we're done
	if (was_dead)
		return;

	// invoke the pain callback
	if ((damage_health || knockback) && target->locals.Pain)
		target->locals.Pain(target, attacker, damage_health, knockback);

	// add to the damage inflicted on a player this frame
	if (client) {
		client->locals.damage_armor += damage_armor;
		client->locals.damage_health += damage_health;

		vec_t kick = (damage_armor + damage_health) / 30.0;

		if (kick > 1.0) {
			kick = 1.0;
		}

		G_ClientDamageKick(target, dir, kick);

		if (attacker->client && attacker->client != client) {
			attacker->client->locals.damage_inflicted += damage_health + damage_armor;
		}
	}
}

/**
 * @brief
 */
void G_RadiusDamage(g_entity_t *inflictor, g_entity_t *attacker, g_entity_t *ignore, int16_t damage,
		int16_t knockback, vec_t radius, uint32_t mod) {

	g_entity_t *ent = NULL;

	while ((ent = G_FindRadius(ent, inflictor->s.origin, radius)) != NULL) {
		vec3_t dir;

		if (ent == ignore)
			continue;

		if (!ent->locals.take_damage)
			continue;

		VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
		const vec_t dist = VectorNormalize(dir);

		vec_t d = damage - 0.5 * dist;
		const vec_t k = knockback - 0.5 * dist;

		if (d <= 0 && k <= 0) // too far away to be damaged
			continue;

		if (ent == attacker) { // reduce self damage
			if (mod == MOD_BFG_BLAST)
				d = d * 0.25;
			else
				d = d * 0.5;
		}

		if (!G_CanDamage(ent, inflictor))
			continue;

		G_Damage(ent, inflictor, attacker, dir, NULL, NULL, d, k, DMG_RADIUS, mod);
	}
}
