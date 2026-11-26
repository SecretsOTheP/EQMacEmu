#!/usr/bin/env python3
"""Compile only extracted production access methods against deterministic timer/DB stubs."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'zone/zoning.cpp').read_text()
def between(a,b): return s[s.index(a):s.index(b,s.index(a))]
production=between('namespace {','void Client::Handle_OP_ZoneChange')
production+=between('void Client::BootFromGuildInstance(', 'void Client::GoToDeath()')
production+=between('bool Client::CanBeInZone(', 'void Client::UpdateZoneChangeCount(')
a=s.index('const bool accepted_planar_rez =');b=s.index(';',a)+1
production+='\nconstexpr int ZoneSolicited=1;\nbool AcceptedRez(int zone_mode,uint32 rez_zone,uint32 rez_guild,uint32 target_zone_id,uint32 target_zone_guild_id){\n'+s[a:b]+'\nreturn accepted_planar_rez;\n}\n'
packet=(root/'zone/client_packet.cpp').read_text();process=(root/'zone/client_process.cpp').read_text();zone=(root/'zone/zone.cpp').read_text();client=(root/'zone/client.cpp').read_text()
assert 'if (!CheckZoneAccessWithGrace())' in packet
assert packet.index('ReturnToTranquility();') < packet.index('Kicking char from zone, not allowed here')
assert 'Connected() && !IsLD() && !dead && !zoning &&' in process
assert 'pop_raid_willing_check_timer.Check()' in process
assert 'pop_raid_willing_grace_timer(180000)' in client
assert 'pop_raid_willing_grace_timer.Disable();' in client
assert zone.count('BootFromGuildInstance(false, true)')==2
assert zone.count('BootFromGuildInstance();')>=2 # DLL checks remain strict
rez=process[process.index('void Client::OPRezzAnswer'):process.index('void Client::OPTGB')]
assert rez.index('if (Action == 1)')<rez.index('pop_raid_willing_rez_zone = PendingRezzZoneID;')
assert rez.index('MovePCGuildID(PendingRezzZoneID')<rez.index('pop_raid_willing_rez_zone = PendingRezzZoneID;')
move=between('void Client::ProcessMovePC(', 'void Client::BootFromGuildInstance(')
assert 'pop_raid_willing_rez_zone = 0;' in move
entry=between('void Client::Handle_OP_ZoneChange', 'void Client::SendZoneCancel')
assert entry.index('pop_raid_willing_rez_zone = 0;')<entry.index('const bool accepted_planar_rez')
assert entry.index('GetLevel() < minlevel')<entry.index('const bool accepted_planar_rez')
assert entry.index('zonesummon_id == rez_zone && zonesummon_guildid == rez_guild') < entry.index('/* Check for Valid Zone */')
assert 'target_zone_guild_id = rez_guild;' in entry
assert '!CanRaidWillIntoPopZone(target_zone_id) && !accepted_planar_rez' in entry
assert 'CanEnterPvpInstance(zone_id)' in s # quake/PvP admission remains
with tempfile.TemporaryDirectory(prefix='pop-grace-test-') as td:
 p=Path(td);(p/'production.inc').write_text(production)
 (p/'test.cpp').write_text((root/'tests/pop_raid_willing_grace_test.cpp').read_text())
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Wno-unused-parameter','-Wno-misleading-indentation',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS login/process/combat/rez wiring checks; no server build or database writes')
