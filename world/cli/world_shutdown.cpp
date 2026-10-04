#include "../world_shutdown.h"
#include "../../common/path_manager.h"
#include "../../common/shutdown_duration.h"
#include "../../common/strings.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

// Talks to the world server running out of this directory through a request file it checks every
// few seconds, so it works without telnet and from any shell, cron job or deploy script.
void WorldserverCLI::WorldShutdownCommand(int argc, char **argv, argh::parser &cmd, std::string &description)
{
	description = "Schedules, cancels or checks a shutdown of the running world, its zones and launcher";

	if (cmd[{"-h", "--help"}]) {
		return;
	}

	auto usage = []() {
		std::cout
			<< "Usage: world world:shutdown <delay> [interval]   schedule a shutdown\n"
			<< "       world world:shutdown now                  shut down immediately\n"
			<< "       world world:shutdown cancel               cancel a scheduled shutdown\n"
			<< "       world world:shutdown status               show the current schedule\n\n"
			<< "  delay/interval: 90, 90s, 15m, 1h30m, 2d4h (no spaces; a bare number is seconds)\n"
			<< "  interval: announcement spacing above 15 minutes left (default automatic).\n"
			<< "            Below 15 minutes announcements escalate: 15, 10, 5, 4, 3, 2, 1 minutes, 30 and 10 seconds.\n"
			<< "  Run from the server directory (where eqemu_config.json is).\n";
		std::exit(1);
	};

	std::vector<std::string> args;
	for (int i = 2; i < argc; i++) {
		args.emplace_back(argv[i]);
	}

	if (args.empty() || args.size() > 2) {
		usage();
	}

	const char *user = std::getenv("USER");
	if (!user) {
		user = std::getenv("USERNAME");
	}
	const std::string by = fmt::format("command line ({})", user ? user : "unknown");

	const auto sub = Strings::ToLower(args[0]);
	std::string request;
	if (sub == "now" || sub == "cancel" || sub == "status") {
		request = fmt::format("{} {}", sub, by);
	}
	else if (sub == "disable") {
		request = fmt::format("cancel {}", by);
	}
	else {
		uint32 seconds = 0, interval = 0;
		if (!ShutdownDuration::Parse(args[0], seconds) || seconds == 0) {
			std::cout << "Invalid delay [" << args[0] << "]\n\n";
			usage();
		}
		if (args.size() == 2 && !ShutdownDuration::Parse(args[1], interval)) {
			std::cout << "Invalid interval [" << args[1] << "]\n\n";
			usage();
		}
		request = fmt::format("schedule {} {} {}", seconds, interval, by);
	}

	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path request_file  = fs::path(path.GetServerPath()) / WorldShutdown::RequestFile;
	const fs::path response_file = fs::path(path.GetServerPath()) / WorldShutdown::ResponseFile;

	fs::remove(response_file, ec);

	// Write then rename so world never reads a partial request.
	const auto tmp = request_file.string() + ".tmp";
	{
		std::ofstream out(tmp, std::ios::trunc);
		out << request << "\n";
		if (!out) {
			std::cout << "Could not write [" << tmp << "]\n";
			std::exit(1);
		}
	}
	fs::rename(tmp, request_file, ec);
	if (ec) {
		std::cout << "Could not write [" << request_file.string() << "]: " << ec.message() << "\n";
		std::exit(1);
	}

	// World checks for the request every PollIntervalMS; allow up to three checks before giving up.
	const auto give_up = std::chrono::steady_clock::now() + std::chrono::milliseconds(WorldShutdown::PollIntervalMS * 3);
	std::cout << "Waiting for world to pick up the request (it checks every " << WorldShutdown::PollIntervalMS / 1000 << " seconds)..." << std::endl;
	while (std::chrono::steady_clock::now() < give_up) {
		if (fs::exists(response_file, ec)) {
			std::string response, line;
			{
				std::ifstream in(response_file);
				while (std::getline(in, line)) {
					response += line;
				}
			}
			fs::remove(response_file, ec);
			std::cout << response << std::endl;
			std::exit(response.rfind("ERROR", 0) == 0 ? 1 : 0);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Don't leave the request behind for a world that boots later.
	fs::remove(request_file, ec);
	std::cout
		<< "No response from world. Is world running from [" << path.GetServerPath() << "]?\n"
		<< "The request was withdrawn.\n";
	std::exit(1);
}
