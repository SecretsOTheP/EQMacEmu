#include "../client.h"
#include "../worldserver.h"
#include "../../common/shutdown_duration.h"
extern WorldServer worldserver;

namespace {
	struct WorldScheduleCommand {
		const char *name;           // "#worldshutdown"
		const char *what;           // first help line
		const char *noun;           // "shut down", "restart", "rolling zone restart"
		uint8       schedule_action;
		uint8       now_action;
		uint8       cancel_action;
		uint8       status_action;
	};

	// Shared by #worldshutdown, #worldrestart and #rollingrestart. World owns the schedule and replies to us.
	void SendWorldScheduleCommand(Client *c, const Seperator *sep, const WorldScheduleCommand &cmd)
	{
		if (!worldserver.Connected()) {
			c->Message(Chat::White, "Error: World server disconnected");
			return;
		}

		auto usage = [c, &cmd]() {
			c->Message(Chat::White, fmt::format("{} - {}", cmd.name, cmd.what).c_str());
			c->Message(Chat::White, fmt::format("Usage: {} [when] - Schedule a {}.", cmd.name, cmd.noun).c_str());
			c->Message(Chat::White, "  [when]: a delay (90m, 1h30m, 2d4h; a bare number is seconds), an Eastern time (7am, 7:30pm), a date and time in Eastern (10/5/2026 7am, Oct 5 2026 7:00am, 2026-10-05 07:00), or a Unix timestamp.");
			c->Message(Chat::White, "  Warnings: MOTD only until 60 minutes remain, then every 5 minutes, then every minute from 15 minutes.");
			c->Message(Chat::White, fmt::format("Usage: {} now - Starts the {} immediately.", cmd.name, cmd.noun).c_str());
			c->Message(Chat::White, fmt::format("Usage: {} disable - Cancels a scheduled {}.", cmd.name, cmd.noun).c_str());
			c->Message(Chat::White, fmt::format("Usage: {} status - Shows the scheduled {}.", cmd.name, cmd.noun).c_str());
		};

		const std::string sub = Strings::ToLower(sep->arg[1]);
		if (sub.empty()) {
			usage();
			return;
		}

		ServerPacket pack(ServerOP_ShutdownAll, sizeof(WorldShutDown_Struct));
		auto wsd = (WorldShutDown_Struct*)pack.pBuffer;
		strn0cpy(wsd->admin_name, c->GetName(), sizeof(wsd->admin_name));

		if (sub == "now") {
			wsd->action = cmd.now_action;
		}
		else if (sub == "disable" || sub == "cancel") {
			wsd->action = cmd.cancel_action;
		}
		else if (sub == "status") {
			wsd->action = cmd.status_action;
		}
		else {
			uint32      seconds = 0;
			std::string error;
			if (!ShutdownDuration::ParseWhen(sep->argplus[1], std::time(nullptr), seconds, error)) {
				c->Message(Chat::White, error.c_str());
				usage();
				return;
			}

			wsd->action  = cmd.schedule_action;
			wsd->seconds = seconds;
		}

		worldserver.SendPacket(&pack);
	}
}

void command_worldshutdown(Client *c, const Seperator *sep){
	SendWorldScheduleCommand(c, sep, {
		"#worldshutdown", "Shuts down the server, all zones and the launcher.", "shut down",
		WorldShutDownSchedule, WorldShutDownNow, WorldShutDownCancel, WorldShutDownStatus
	});
}

void command_worldrestart(Client *c, const Seperator *sep){
	// Cancel and status are shared with #worldshutdown: there is one world schedule.
	SendWorldScheduleCommand(c, sep, {
		"#worldrestart", "Shuts down the server and all zones like #worldshutdown, then brings everything back up.", "restart",
		WorldRestartSchedule, WorldRestartNow, WorldShutDownCancel, WorldShutDownStatus
	});
}

void command_rollingrestart(Client *c, const Seperator *sep){
	SendWorldScheduleCommand(c, sep, {
		"#rollingrestart", "Restarts zones a few at a time while world and logins stay up. Players in a restarting zone are briefly disconnected.", "rolling zone restart",
		RollingRestartSchedule, RollingRestartNow, RollingRestartCancel, RollingRestartStatus
	});
}
