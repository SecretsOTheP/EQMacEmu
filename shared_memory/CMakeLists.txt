CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

SET(shared_memory_sources
	items.cpp
	main.cpp
	npc_faction.cpp
	spells.cpp
	skill_caps.cpp
)

SET(shared_memory_headers
	items.h
	npc_faction.h
	spells.h
	skill_caps.h
)

ADD_EXECUTABLE(shared_memory ${shared_memory_sources} ${shared_memory_headers})

INSTALL(TARGETS shared_memory RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

TARGET_LINK_LIBRARIES(shared_memory ${SERVER_LIBS})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
