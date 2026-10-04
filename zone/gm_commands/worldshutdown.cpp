#include "../client.h"
#include "../worldserver.h"
#include "../../common/shutdown_duration.h"
extern WorldServer worldserver;

void command_worldshutdown(Client *c, const Seperator *sep){
	// GM command to shutdown world server and all zones. World owns the schedule and replies to us.
	if (!worldserver.Connected()) {
		c->Message(Chat::White, "Error: World server disconnected");
		return;
	}

	auto usage = [c]() {
		c->Message(Chat::White, "#worldshutdown - Shuts down the server, all zones and the launcher.");
		c->Message(Chat::White, "Usage: #worldshutdown [delay] [interval] - Shut down after [delay]. Announcements every [interval] until 15 minutes remain, then they escalate.");
		c->Message(Chat::White, "  [delay]/[interval]: 90, 90s, 15m, 1h30m, 2d4h (no spaces; a bare number is seconds). [interval] is optional.");
		c->Message(Chat::White, "Usage: #worldshutdown now - Shuts down immediately.");
		c->Message(Chat::White, "Usage: #worldshutdown disable - Cancels a scheduled shut down.");
		c->Message(Chat::White, "Usage: #worldshutdown status - Shows the scheduled shut down.");
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
		wsd->action = WorldShutDownNow;
	}
	else if (sub == "disable" || sub == "cancel") {
		wsd->action = WorldShutDownCancel;
	}
	else if (sub == "status") {
		wsd->action = WorldShutDownStatus;
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

		wsd->action           = WorldShutDownSchedule;
		wsd->seconds          = seconds;
		wsd->interval_seconds = interval;
	}

	worldserver.SendPacket(&pack);
}
