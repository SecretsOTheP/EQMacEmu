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

#ifndef __EQEMU_TESTS_WORLD_QUEUE_H
#define __EQEMU_TESTS_WORLD_QUEUE_H

#include "cppunit/cpptest.h"
#include "../world/world_queue.h"

class WorldQueueTest : public Test::Suite {
	typedef void(WorldQueueTest::*TestFunction)(void);
public:
	WorldQueueTest() {
		TEST_ADD(WorldQueueTest::AdmitBelowCap);
		TEST_ADD(WorldQueueTest::QueueOrderByFirstClick);
		TEST_ADD(WorldQueueTest::RepeatPlayKeepsPosition);
		TEST_ADD(WorldQueueTest::EntryExpiresWithoutRefresh);
		TEST_ADD(WorldQueueTest::AdmitWhenEveryoneAheadFits);
		TEST_ADD(WorldQueueTest::ReservationCountsUntilInZone);
		TEST_ADD(WorldQueueTest::ReservationRefreshedByRepeatPlay);
		TEST_ADD(WorldQueueTest::ReservationExpiryReleasesSlot);
		TEST_ADD(WorldQueueTest::ReservationHeldWhileAtCharSelect);
		TEST_ADD(WorldQueueTest::GraceHoldsSlotAndAdmits);
		TEST_ADD(WorldQueueTest::GraceExpires);
		TEST_ADD(WorldQueueTest::GraceHeldWhileAtCharSelect);
		TEST_ADD(WorldQueueTest::GraceConsumedOnReturn);
		TEST_ADD(WorldQueueTest::LinkdeadOverlapCountsOnce);
		TEST_ADD(WorldQueueTest::LinkdeadStampStartsGrace);
		TEST_ADD(WorldQueueTest::ClaimSlotAtEnterWorld);
		TEST_ADD(WorldQueueTest::RekeyNewAccount);
		TEST_ADD(WorldQueueTest::PositionAndSize);
	}

	~WorldQueueTest() {
	}

private:
	static WorldQueueConfig Cfg(uint32 cap) {
		WorldQueueConfig c;
		c.cap = cap;
		c.entry_timeout_s = 60;
		c.slot_hold_s = 300;
		return c;
	}

	// Accounts in a zone, nobody at character select.
	static QueuePopulation Zone(std::initializer_list<uint32> in_zone) {
		QueuePopulation p;
		p.in_zone = AccountSet(in_zone);
		return p;
	}

	// Accounts in a zone plus accounts connected at character select.
	static QueuePopulation Pop(std::initializer_list<uint32> in_zone, std::initializer_list<uint32> at_char_select) {
		QueuePopulation p;
		p.in_zone = AccountSet(in_zone);
		p.at_char_select = AccountSet(at_char_select);
		return p;
	}

	void AdmitBelowCap() {
		WorldQueue q;
		q.SetConfig(Cfg(2));
		auto d = q.Decide(101, 1, 0, Zone({}), 1000);
		TEST_ASSERT(d.admit);
		TEST_ASSERT_EQUALS(0u, d.position);
		TEST_ASSERT(q.HasReservation(1));
		TEST_ASSERT_EQUALS(0u, (uint32)q.Entries().size());
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({})));
	}

	void QueueOrderByFirstClick() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		QueuePopulation z = Zone({ 9 });
		auto a = q.Decide(101, 1, 0, z, 1000);
		auto b = q.Decide(102, 2, 0, z, 1001);
		auto c = q.Decide(103, 3, 0, z, 1002);
		TEST_ASSERT(!a.admit && !b.admit && !c.admit);
		TEST_ASSERT_EQUALS(1u, a.position);
		TEST_ASSERT_EQUALS(2u, b.position);
		TEST_ASSERT_EQUALS(3u, c.position);
		TEST_ASSERT_EQUALS(3u, c.queue_size);
	}

	void RepeatPlayKeepsPosition() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		QueuePopulation z = Zone({ 9 });
		q.Decide(101, 1, 0, z, 1000);
		q.Decide(102, 2, 0, z, 1001);
		auto a = q.Decide(101, 1, 0, z, 1030);
		TEST_ASSERT_EQUALS(1u, a.position);
		TEST_ASSERT_EQUALS(2u, a.queue_size);
		TEST_ASSERT_EQUALS(1000u, q.Entries()[0].joined_at);
		TEST_ASSERT_EQUALS(1030u, q.Entries()[0].last_seen);
	}

	void EntryExpiresWithoutRefresh() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		QueuePopulation z = Zone({ 9 });
		q.Decide(101, 1, 0, z, 1000);
		q.Decide(102, 2, 0, z, 1001);
		// 2 keeps polling, 1 stops
		q.Decide(102, 2, 0, z, 1040);
		q.Tick(z, 1061);
		TEST_ASSERT_EQUALS(1u, (uint32)q.Entries().size());
		TEST_ASSERT_EQUALS(1u, q.Position(2));
		TEST_ASSERT_EQUALS(0u, q.Position(1));
		// 1 comes back at the end
		auto a = q.Decide(101, 1, 0, z, 1070);
		TEST_ASSERT_EQUALS(2u, a.position);
	}

	void AdmitWhenEveryoneAheadFits() {
		WorldQueue q;
		q.SetConfig(Cfg(3));
		QueuePopulation z = Zone({ 9 });
		q.Decide(101, 1, 0, z, 1000);          // admitted (1 in zone, 1 reserved)
		TEST_ASSERT(q.HasReservation(1));
		q.SetConfig(Cfg(2));                   // now full: 1 in zone + 1 reservation
		auto b = q.Decide(102, 2, 0, z, 1001); // queued 1
		auto c = q.Decide(103, 3, 0, z, 1002); // queued 2
		TEST_ASSERT(!b.admit && !c.admit);
		q.SetConfig(Cfg(4));                   // two free slots
		// c is second in line, but both fit, so c does not have to wait for b's poll
		auto c2 = q.Decide(103, 3, 0, z, 1003);
		TEST_ASSERT(c2.admit);
		auto b2 = q.Decide(102, 2, 0, z, 1004);
		TEST_ASSERT(b2.admit);
		TEST_ASSERT_EQUALS(4u, q.EffectivePopulation(z));
	}

	void ReservationCountsUntilInZone() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.Decide(101, 1, 0, Zone({}), 1000);
		TEST_ASSERT(q.HasReservation(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({})));
		auto b = q.Decide(102, 2, 0, Zone({}), 1001);
		TEST_ASSERT(!b.admit);
		// 1 enters a zone: the reservation is consumed, the account counts once
		q.Tick(Zone({ 1 }), 1010);
		TEST_ASSERT(!q.HasReservation(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({ 1 })));
	}

	void ReservationRefreshedByRepeatPlay() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.Decide(101, 1, 0, Zone({}), 1000);
		auto again = q.Decide(101, 1, 0, Zone({}), 1200);
		TEST_ASSERT(again.admit);
		TEST_ASSERT_EQUALS(1500u, q.Reservations().at(1).expires_at);
		TEST_ASSERT_EQUALS(0u, (uint32)q.Entries().size());
	}

	void ReservationExpiryReleasesSlot() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.Decide(101, 1, 0, Zone({}), 1000);   // admitted, never connects to world
		for (uint32 t = 1001; t < 1300; t += 30) {
			TEST_ASSERT(!q.Decide(102, 2, 0, Zone({}), t).admit);
		}
		q.Tick(Zone({}), 1300);
		TEST_ASSERT(!q.HasReservation(1));
		TEST_ASSERT_EQUALS(0u, q.EffectivePopulation(Zone({})));
		// the slot is released, nothing is re-queued for 1
		TEST_ASSERT_EQUALS(0u, q.Position(1));
		auto b = q.Decide(102, 2, 0, Zone({}), 1301);
		TEST_ASSERT(b.admit);
		TEST_ASSERT_EQUALS(0u, (uint32)q.Entries().size());
	}

	void ReservationHeldWhileAtCharSelect() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.Decide(101, 1, 0, Zone({}), 1000);
		// sits at character select far beyond the timeout: still held, still counted
		q.Tick(Pop({}, { 1 }), 2000);
		q.Tick(Pop({}, { 1 }), 3000);
		TEST_ASSERT(q.HasReservation(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Pop({}, { 1 })));
		TEST_ASSERT(!q.Decide(102, 2, 0, Pop({}, { 1 }), 3001).admit);
		// leaves character select right after that poll at 3001: the 300 s countdown starts there
		q.Tick(Zone({}), 3300);
		TEST_ASSERT(q.HasReservation(1));
		q.Tick(Zone({}), 3301);
		TEST_ASSERT(!q.HasReservation(1));
	}

	void GraceHoldsSlotAndAdmits() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.AddGrace(1, 1000);
		TEST_ASSERT(q.HasGrace(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({})));
		auto b = q.Decide(102, 2, 0, Zone({}), 1001);
		TEST_ASSERT(!b.admit);
		auto a = q.Decide(101, 1, 0, Zone({}), 1002);
		TEST_ASSERT(a.admit);
		TEST_ASSERT(!q.HasGrace(1));
		TEST_ASSERT(q.HasReservation(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({})));
	}

	void GraceExpires() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.AddGrace(1, 1000);
		q.Tick(Zone({}), 1299);
		TEST_ASSERT(q.HasGrace(1));
		q.Tick(Zone({}), 1300);
		TEST_ASSERT(!q.HasGrace(1));
		TEST_ASSERT_EQUALS(0u, q.EffectivePopulation(Zone({})));
	}

	void GraceHeldWhileAtCharSelect() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.AddGrace(1, 1000);  // camped to character select
		q.Tick(Pop({}, { 1 }), 1500);
		q.Tick(Pop({}, { 1 }), 2000);
		TEST_ASSERT(q.HasGrace(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Pop({}, { 1 })));
		// picks another character: grace is consumed on zone-in
		q.Tick(Zone({ 1 }), 2010);
		TEST_ASSERT(!q.HasGrace(1));
		// or leaves character select at 2000 without entering: 300 s from there
		q.AddGrace(3, 1000);
		q.Tick(Pop({}, { 3 }), 2000);
		q.Tick(Zone({}), 2299);
		TEST_ASSERT(q.HasGrace(3));
		q.Tick(Zone({}), 2300);
		TEST_ASSERT(!q.HasGrace(3));
	}

	void GraceConsumedOnReturn() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.AddGrace(1, 1000);
		q.Tick(Zone({ 1 }), 1010);
		TEST_ASSERT(!q.HasGrace(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({ 1 })));
	}

	void LinkdeadOverlapCountsOnce() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		// account 1 is linkdead in a zone and reconnects: admitted on its own slot, no reservation
		auto a = q.Decide(101, 1, 0, Zone({ 1 }), 1000);
		TEST_ASSERT(a.admit);
		TEST_ASSERT(!q.HasReservation(1));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({ 1 })));
		// grace plus in-zone for the same account is still one
		q.AddGrace(1, 1001);
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({ 1 })));
		// reservation plus grace for one account is one
		q.Tick(Zone({}), 1002); // consumes nothing: 1 not in zone now, grace stays
		q.Decide(101, 1, 0, Zone({}), 1003); // grace -> reservation
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({})));
	}

	void LinkdeadStampStartsGrace() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		q.NoteLinkdead(1, 1000);
		q.NoteLinkdead(1, 1020); // later stamps do not move the start
		q.AddGrace(1, 1040);     // zone removed the client 40s after the disconnect
		TEST_ASSERT_EQUALS(1000u, q.Grace().at(1).started_at);
		TEST_ASSERT_EQUALS(1300u, q.Grace().at(1).expires_at);
		// a cleared stamp is ignored
		q.Clear();
		q.NoteLinkdead(2, 1000);
		q.ClearLinkdead(2);
		q.AddGrace(2, 1040);
		TEST_ASSERT_EQUALS(1340u, q.Grace().at(2).expires_at);
		// a stamp older than the whole grace window produces no grace at all
		q.Clear();
		q.NoteLinkdead(3, 1000);
		q.AddGrace(3, 1400);
		TEST_ASSERT(!q.HasGrace(3));
	}

	void ClaimSlotAtEnterWorld() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		// free slot: claimed with a bridging reservation
		TEST_ASSERT(q.ClaimSlot(101, 1, 0, Zone({}), 1000));
		TEST_ASSERT(q.HasReservation(1));
		// full: refused and queued so the place is kept for the Play that follows
		TEST_ASSERT(!q.ClaimSlot(102, 2, 0, Zone({}), 1001));
		TEST_ASSERT_EQUALS(1u, q.Position(2));
		// a slot holder is always allowed
		q.AddGrace(3, 1002);
		TEST_ASSERT(q.ClaimSlot(103, 3, 0, Zone({}), 1003));
		TEST_ASSERT(q.ClaimSlot(104, 4, 0, Zone({ 4 }), 1004));
	}

	void RekeyNewAccount() {
		WorldQueue q;
		q.SetConfig(Cfg(1));
		// first-time account: admitted under a provisional id because it has no world id yet
		uint32 provisional = WorldQueue::ProvisionalAccountId(101);
		TEST_ASSERT(provisional != 101u);
		TEST_ASSERT(q.Decide(101, provisional, 0, Zone({}), 1000).admit);
		TEST_ASSERT(q.HasReservation(provisional));
		// an existing world account whose id equals the LS id is unaffected
		TEST_ASSERT(!q.HasReservation(101));
		// world creates account 57 at auth and moves the slot
		q.Rekey(provisional, 57);
		TEST_ASSERT(!q.HasReservation(provisional));
		TEST_ASSERT(q.HasReservation(57));
		TEST_ASSERT_EQUALS(101u, q.Reservations().at(57).ls_account_id);
		// enter world under the real id goes through, and zone-in consumes it
		TEST_ASSERT(q.ClaimSlot(101, 57, 0, Pop({}, { 57 }), 1010));
		q.Tick(Zone({ 57 }), 1020);
		TEST_ASSERT(!q.HasReservation(57));
		TEST_ASSERT_EQUALS(1u, q.EffectivePopulation(Zone({ 57 })));
		// queued entries move too
		q.Clear();
		uint32 provisional2 = WorldQueue::ProvisionalAccountId(202);
		q.Decide(202, provisional2, 0, Zone({ 9 }), 2000);
		q.Rekey(provisional2, 58);
		TEST_ASSERT_EQUALS(1u, q.Position(58));
		TEST_ASSERT_EQUALS(0u, q.Position(provisional2));
		// no-ops
		q.Rekey(58, 58);
		q.Rekey(58, 0);
		TEST_ASSERT_EQUALS(1u, q.Position(58));
	}

	void PositionAndSize() {
		WorldQueue q;
		q.SetConfig(Cfg(0));
		auto a = q.Decide(101, 1, 0, Zone({}), 1000);
		q.Decide(102, 2, 0, Zone({}), 1000);
		auto c = q.Decide(103, 3, 0, Zone({}), 1000);
		TEST_ASSERT_EQUALS(1u, a.position);
		TEST_ASSERT_EQUALS(1u, a.queue_size);
		TEST_ASSERT_EQUALS(3u, c.position);
		TEST_ASSERT_EQUALS(3u, c.queue_size);
	}
};

#endif
