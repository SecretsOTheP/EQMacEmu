#!/usr/bin/env python3
"""Generate a standalone test using the production factory and string helper.

Run with the server's Lua 5.1/luabind development dependencies; see README.txt.
The zone/entity doubles model ownership without loading a database or zone.
"""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
source = (root / 'zone/lua_general.cpp').read_text()
factory = source[source.index('#define LuaCreateNPCParse('):source.index('\nint get_rulei(')]
strings = (root / 'common/strings.cpp').read_text()
start = strings.index('char* strn0cpy(')
string_helper = strings[start:strings.index('\nbool strn0cpyt', start)]
output = Path(sys.argv[1])
output.write_text((Path(__file__).parent / 'prefix.cpp').read_text() + '\n' + string_helper + '\n' + factory + '\n' + (Path(__file__).parent / 'checks.cpp').read_text())
