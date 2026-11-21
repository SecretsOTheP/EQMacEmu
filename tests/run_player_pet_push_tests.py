#!/usr/bin/env python3
"""Prepare isolated C++ tests of the production policy, command and receiver bodies.
Usage: python3 tests/run_player_pet_push_tests.py OUTPUT_DIRECTORY FMT_INCLUDE_DIRECTORY
Compile only this small harness; no server binaries or running realm are modified.
"""
from pathlib import Path
import shutil, sys
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
for name in ['common/player_pet_push.h','zone/gm_commands/petpush.cpp']:
 target=out/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/name,target)
shutil.copyfile(root/'tests/player_pet_push_stubs.h',out/'test_stubs.h')
shutil.copyfile(root/'tests/player_pet_push_test.cpp',out/'test.cpp')
shutil.copytree(Path(sys.argv[2])/'fmt',out/'fmt',dirs_exist_ok=True)
for name in ['zone/client.h','zone/command.h','zone/worldserver.h','common/rulesys.h']:
 (out/name).write_text('#include "test_stubs.h"\n')
for file,function,output in [('zone/worldserver.cpp','Receive','receiver.inc'),('world/zoneserver.cpp','WorldReceive','world_receiver.inc')]:
 source=(root/file).read_text();start=source.index('case ServerOP_PlayerPetPushUpdate: {');end=source.index('case ServerOP_InstanceRespawnToggle:',start)
 (out/output).write_text('void '+function+'(ServerPacket *pack) { switch(pack->opcode) {\n'+source[start:end]+'default: break; } }\n')
attack=(root/'zone/attack.cpp').read_text()
a=attack.index('a->force = PlayerPetPush::Scale(');b=attack.index('CastToNPC()->AddPush(',a)
assert a<b and 'a->force' in attack[b:b+100]
command=(root/'zone/command.cpp').read_text()
assert 'AccountStatus::GMImpossible, command_petpush' in command
assert '#include "gm_commands/petpush.cpp"' in command
rules=(root/'common/ruletypes.h').read_text();assert 'PlayerMeleePushMultiplier, 0.25,' in rules
protocol=(root/'common/servertalk.h').read_text();assert '#define ServerOP_PlayerPetPushUpdate 0x4206' in protocol
assert 'struct ServerPlayerPetPushUpdate_Struct { uint32 ruleset_id; };' in protocol
print('Prepared production-code tests; command access and packet/movement integration checks passed.')
