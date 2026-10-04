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
			c->Message(Chat::White, fmt::format("Usage: {} [delay] [interval] - Schedule a {} after [delay]. Announcements every [interval] until 15 minutes remain, then they escalate.", cmd.name, cmd.noun).c_str());
			c->Message(Chat::White, "  [delay]/[interval]: 90, 90s, 15m, 1h30m, 2d4h (no spaces; a bare number is seconds). [interval] is optional.");
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
			uint32 seconds = 0, interval = 0;
			if (!ShutdownDuration::Parse(sep->arg[1], seconds) || seconds == 0) {
				c->Message(Chat::White, fmt::format("Invalid delay '{}'.", sep->arg[1]).c_str());
				usage();
				return;
			}
			if (sep->arg[2][0] && !ShutdownDuration::Parse(sep->arg[2], interval)) {
				c->Message(Chat::White, fmt::format("Invalid interval '{}'.", sep->arg[2]).c_str());
				usage();
				return;
			}

			wsd->action           = cmd.schedule_action;
			wsd->seconds          = seconds;
			wsd->interval_seconds = interval;
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
