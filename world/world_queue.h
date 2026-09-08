/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2025 EQEMu Development Team (http://eqemu.org)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#ifndef WORLD_QUEUE_H_
#define WORLD_QUEUE_H_

#include "../common/types.h"
#include <functional>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

/*
 * Login queue bookkeeping for the world server. Just the logic (no database/packets/rules/etc),
 * so it can be driven from unit tests. Times are unix seconds supplied by the caller.
 *
 * Slot accounting is per world account. An account holds a slot while it is in a zone, while it
 * holds a reservation (admitted, not yet in a zone) or while it is in grace (recently left a zone).
 * Both are held for as long as the account has a connected character-select session and expire
 * slot_hold_s after that session is gone; these are only labels so logs and the console can say
 * which state an account is in. The client list supplies both account sets on every call.
 */

struct WorldQueueConfig {
	uint32 cap = 0;                     // Quarm:PlayerPopulationCap
	uint32 entry_timeout_s = 60;        // Quarm:QueueEntryTimeoutS
	uint32 slot_hold_s = 300;           // Quarm:QueueSlotHoldS, for reservation/grace
};

typedef std::unordered_set<uint32> AccountSet;

// World account ids as seen by the client list. Status > 0 accounts and offline traders are left out.
struct QueuePopulation {
	AccountSet in_zone;         // at least one entry zoning or in a zone
	AccountSet at_char_select;  // connected to world, no entry in a zone
};

struct QueueEntry {
	uint32 ls_account_id;
	uint32 world_account_id;
	uint32 ip;
	uint32 joined_at;
	uint32 last_seen;
};

struct QueueReservation {
	uint32 ls_account_id;
	uint32 ip;
	uint32 expires_at;
};

struct QueueGrace {
	uint32 started_at;
	uint32 expires_at;
};

struct QueueDecision {
	bool   admit;
	uint32 position;    // 1-based; 0 when admitted
	uint32 queue_size;
};

class WorldQueue {
public:
	typedef std::function<void(const std::string&)> LogFn;

	// Rules are re-read by the caller on every use; the config is a copy of the current values.
	void SetConfig(const WorldQueueConfig& c) { m_config = c; }
	const WorldQueueConfig& Config() const { return m_config; }
	// Receives one line per state change ("[Queue] account ... joined at position 3"). Unset in tests.
	void SetLog(LogFn fn) { m_log = std::move(fn); }

	// Answer a play request. Refreshes or creates the queue entry, admits when the account
	// already holds a slot or when every entry ahead of it also fits in the free slots.
	QueueDecision Decide(uint32 ls_account_id, uint32 world_account_id, uint32 ip, const QueuePopulation& pop, uint32 now);

	// Expire stale entries, reservations and grace; consume reservations and grace of accounts that
	// reached a zone; keep those of accounts still at character select alive. Called periodically and
	// at the start of every Decide.
	void Tick(const QueuePopulation& pop, uint32 now);

	// Claim a slot for an account about to enter a zone from character select. True when the account
	// already holds one or the world has room (a reservation bridges the gap until the zone reports).
	// False enqueues the account so it keeps its place when it comes back through server select.
	bool ClaimSlot(uint32 ls_account_id, uint32 world_account_id, uint32 ip, const QueuePopulation& pop, uint32 now);

	// A first-time account has no world id until world creates it at authentication. Until then the
	// queue keys it by a provisional id derived from the login-server id, in a range no world id uses,
	// so it can never share an entry or a reservation with an existing account.
	static uint32 ProvisionalAccountId(uint32 ls_account_id) { return 0x80000000u | ls_account_id; }

	// Move whatever a provisional id holds to the account id world just created.
	void Rekey(uint32 old_world_account_id, uint32 new_world_account_id);

	// What the cap is compared against: in-zone accounts plus reservations and grace not already in a zone.
	uint32 EffectivePopulation(const QueuePopulation& pop) const;
	// True when the account already counts toward the population by any of the three states.
	bool   HoldsSlot(uint32 world_account_id, const QueuePopulation& pop) const;
	bool   HasReservation(uint32 world_account_id) const { return m_reservations.count(world_account_id) != 0; }
	bool   HasGrace(uint32 world_account_id) const { return m_grace.count(world_account_id) != 0; }
	uint32 Position(uint32 world_account_id) const; // 1-based place in the queue; 0 when not queued

	// Grace starts at the disconnect. For a linkdead client that is the linkdead stamp, which precedes
	// the eventual removal of the zone entry by the zone's linkdead camp.
	void AddGrace(uint32 world_account_id, uint32 now);      // account's last in-zone entry went away
	void NoteLinkdead(uint32 world_account_id, uint32 now);  // zone reported the client linkdead; first stamp wins
	void ClearLinkdead(uint32 world_account_id);             // zone reported the client alive again

	void Enqueue(uint32 ls_account_id, uint32 world_account_id, uint32 ip, uint32 now); // append; no-op if already queued
	void RemoveEntry(uint32 world_account_id);
	void Clear(); // drop every entry, reservation, grace and linkdead stamp (tests, and nothing else so far)

	// Read-only views for the console and the tests.
	const std::vector<QueueEntry>&              Entries() const { return m_entries; }
	const std::map<uint32, QueueReservation>&   Reservations() const { return m_reservations; }
	const std::map<uint32, QueueGrace>&         Grace() const { return m_grace; }

private:
	void   Emit(const std::string& s) const { if (m_log) m_log(s); } // log through the callback, if any
	size_t FindEntry(uint32 world_account_id) const;                 // index into m_entries, or npos
	void   Reserve(uint32 ls_account_id, uint32 world_account_id, uint32 ip, uint32 now); // create or refresh a reservation

	WorldQueueConfig                    m_config;
	LogFn                               m_log;
	std::vector<QueueEntry>             m_entries;      // queue order
	std::map<uint32, QueueReservation>  m_reservations; // by world account id
	std::map<uint32, QueueGrace>        m_grace;        // by world account id
	std::map<uint32, uint32>            m_linkdead_since;
};

#endif /*WORLD_QUEUE_H_*/
