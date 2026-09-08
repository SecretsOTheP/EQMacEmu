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
#include "world_queue.h"

static const size_t QUEUE_NPOS = (size_t)-1;

size_t WorldQueue::FindEntry(uint32 world_account_id) const
{
	for (size_t i = 0; i < m_entries.size(); i++) {
		if (m_entries[i].world_account_id == world_account_id) {
			return i;
		}
	}
	return QUEUE_NPOS;
}

uint32 WorldQueue::Position(uint32 world_account_id) const
{
	size_t idx = FindEntry(world_account_id);
	return idx == QUEUE_NPOS ? 0 : (uint32)(idx + 1);
}

uint32 WorldQueue::EffectivePopulation(const QueuePopulation& pop) const
{
	uint32 n = (uint32)pop.in_zone.size();
	for (auto& r : m_reservations) {
		if (!pop.in_zone.count(r.first)) {
			n++;
		}
	}
	for (auto& g : m_grace) {
		if (!pop.in_zone.count(g.first) && !m_reservations.count(g.first)) {
			n++;
		}
	}
	return n;
}

bool WorldQueue::HoldsSlot(uint32 world_account_id, const QueuePopulation& pop) const
{
	return pop.in_zone.count(world_account_id) != 0 || HasReservation(world_account_id) || HasGrace(world_account_id);
}

void WorldQueue::Enqueue(uint32 ls_account_id, uint32 world_account_id, uint32 ip, uint32 now)
{
	if (FindEntry(world_account_id) != QUEUE_NPOS) {
		return;
	}
	m_entries.push_back(QueueEntry{ ls_account_id, world_account_id, ip, now, now });
	Emit("[Queue] account " + std::to_string(world_account_id) + " (ls " + std::to_string(ls_account_id)
		+ ") joined at position " + std::to_string(m_entries.size()));
}

void WorldQueue::RemoveEntry(uint32 world_account_id)
{
	size_t idx = FindEntry(world_account_id);
	if (idx != QUEUE_NPOS) {
		m_entries.erase(m_entries.begin() + idx);
	}
}

void WorldQueue::Reserve(uint32 ls_account_id, uint32 world_account_id, uint32 ip, uint32 now)
{
	m_reservations[world_account_id] = QueueReservation{ ls_account_id, ip, now + m_config.slot_hold_s };
}

void WorldQueue::Rekey(uint32 old_world_account_id, uint32 new_world_account_id)
{
	if (old_world_account_id == new_world_account_id || new_world_account_id == 0) {
		return;
	}

	auto res = m_reservations.find(old_world_account_id);
	if (res != m_reservations.end()) {
		if (!m_reservations.count(new_world_account_id)) {
			m_reservations[new_world_account_id] = res->second;
		}
		m_reservations.erase(res);
		Emit("[Queue] reservation moved from account " + std::to_string(old_world_account_id) + " to new account "
			+ std::to_string(new_world_account_id));
	}

	auto gr = m_grace.find(old_world_account_id);
	if (gr != m_grace.end()) {
		if (!m_grace.count(new_world_account_id)) {
			m_grace[new_world_account_id] = gr->second;
		}
		m_grace.erase(gr);
	}

	size_t idx = FindEntry(old_world_account_id);
	if (idx != QUEUE_NPOS) {
		if (FindEntry(new_world_account_id) == QUEUE_NPOS) {
			m_entries[idx].world_account_id = new_world_account_id;
		}
		else {
			m_entries.erase(m_entries.begin() + idx);
		}
	}
}

void WorldQueue::Clear()
{
	m_entries.clear();
	m_reservations.clear();
	m_grace.clear();
	m_linkdead_since.clear();
}

void WorldQueue::Tick(const QueuePopulation& pop, uint32 now)
{
	for (auto it = m_entries.begin(); it != m_entries.end();) {
		if (now > it->last_seen && now - it->last_seen > m_config.entry_timeout_s) {
			Emit("[Queue] account " + std::to_string(it->world_account_id) + " entry expired, not refreshed for "
				+ std::to_string(now - it->last_seen) + "s");
			it = m_entries.erase(it);
		}
		else {
			++it;
		}
	}

	for (auto it = m_reservations.begin(); it != m_reservations.end();) {
		if (pop.in_zone.count(it->first)) {
			Emit("[Queue] account " + std::to_string(it->first) + " reservation consumed, in zone");
			it = m_reservations.erase(it);
		}
		else if (pop.at_char_select.count(it->first)) {
			it->second.expires_at = now + m_config.slot_hold_s;
			++it;
		}
		else if (now >= it->second.expires_at) {
			Emit("[Queue] account " + std::to_string(it->first) + " reservation expired, slot released");
			it = m_reservations.erase(it);
		}
		else {
			++it;
		}
	}

	for (auto it = m_grace.begin(); it != m_grace.end();) {
		if (pop.in_zone.count(it->first)) {
			Emit("[Queue] account " + std::to_string(it->first) + " grace consumed, back in zone");
			it = m_grace.erase(it);
		}
		else if (pop.at_char_select.count(it->first)) {
			it->second.expires_at = now + m_config.slot_hold_s;
			++it;
		}
		else if (now >= it->second.expires_at) {
			Emit("[Queue] account " + std::to_string(it->first) + " grace expired, slot released");
			it = m_grace.erase(it);
		}
		else {
			++it;
		}
	}

	for (auto it = m_linkdead_since.begin(); it != m_linkdead_since.end();) {
		// A linkdead stamp only matters until the grace it would start has run out anyway.
		if (now > it->second && now - it->second > m_config.slot_hold_s) {
			it = m_linkdead_since.erase(it);
		}
		else {
			++it;
		}
	}
}

QueueDecision WorldQueue::Decide(uint32 ls_account_id, uint32 world_account_id, uint32 ip, const QueuePopulation& pop, uint32 now)
{
	Tick(pop, now);

	QueueDecision d{};

	if (pop.in_zone.count(world_account_id)) {
		// The account's own session is still in a zone; it already counts. The login-server handler answers
		// such requests with -4 before asking the queue, so this is only reached by status-exempt paths.
		RemoveEntry(world_account_id);
		d.admit = true;
		Emit("[Queue] account " + std::to_string(world_account_id) + " admitted, own session still in zone");
		return d;
	}

	auto res = m_reservations.find(world_account_id);
	if (res != m_reservations.end()) {
		res->second.expires_at = now + m_config.slot_hold_s;
		RemoveEntry(world_account_id);
		d.admit = true;
		Emit("[Queue] account " + std::to_string(world_account_id) + " admitted, reservation refreshed");
		return d;
	}

	if (m_grace.count(world_account_id)) {
		m_grace.erase(world_account_id);
		Reserve(ls_account_id, world_account_id, ip, now);
		RemoveEntry(world_account_id);
		d.admit = true;
		Emit("[Queue] account " + std::to_string(world_account_id) + " admitted from grace");
		return d;
	}

	// A newcomer's place is behind every current entry; it is only written if it has to wait.
	size_t idx = FindEntry(world_account_id);
	bool   is_new = idx == QUEUE_NPOS;
	if (is_new) {
		idx = m_entries.size();
	}
	else {
		m_entries[idx].last_seen = now;
	}

	uint32 population = EffectivePopulation(pop);
	uint32 free_slots = m_config.cap > population ? m_config.cap - population : 0;
	if (idx < free_slots) {
		if (!is_new) {
			m_entries.erase(m_entries.begin() + idx);
		}
		Reserve(ls_account_id, world_account_id, ip, now);
		d.admit = true;
		Emit("[Queue] account " + std::to_string(world_account_id) + " admitted, population " + std::to_string(population + 1)
			+ "/" + std::to_string(m_config.cap) + ", " + std::to_string(m_entries.size()) + " waiting");
		return d;
	}

	if (is_new) {
		Enqueue(ls_account_id, world_account_id, ip, now);
	}

	d.admit      = false;
	d.position   = (uint32)(idx + 1);
	d.queue_size = (uint32)m_entries.size();
	return d;
}

bool WorldQueue::ClaimSlot(uint32 ls_account_id, uint32 world_account_id, uint32 ip, const QueuePopulation& pop, uint32 now)
{
	Tick(pop, now);

	if (HoldsSlot(world_account_id, pop)) {
		return true;
	}
	if (EffectivePopulation(pop) < m_config.cap) {
		RemoveEntry(world_account_id);
		Reserve(ls_account_id, world_account_id, ip, now);
		Emit("[Queue] account " + std::to_string(world_account_id) + " claimed a free slot at enter world");
		return true;
	}
	Enqueue(ls_account_id, world_account_id, ip, now);
	return false;
}

void WorldQueue::AddGrace(uint32 world_account_id, uint32 now)
{
	if (m_grace.count(world_account_id)) {
		return;
	}
	uint32 started = now;
	auto ld = m_linkdead_since.find(world_account_id);
	if (ld != m_linkdead_since.end()) {
		if (ld->second < now) {
			started = ld->second;
		}
		m_linkdead_since.erase(ld);
	}
	uint32 expires = started + m_config.slot_hold_s;
	if (expires <= now) {
		return;
	}
	m_grace[world_account_id] = QueueGrace{ started, expires };
	Emit("[Queue] account " + std::to_string(world_account_id) + " left zone, grace for " + std::to_string(expires - now) + "s");
}

void WorldQueue::NoteLinkdead(uint32 world_account_id, uint32 now)
{
	if (!m_linkdead_since.count(world_account_id)) {
		m_linkdead_since[world_account_id] = now;
	}
}

void WorldQueue::ClearLinkdead(uint32 world_account_id)
{
	m_linkdead_since.erase(world_account_id);
}
