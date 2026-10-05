#include "../world_shutdown.h"
#include "../../common/path_manager.h"
#include "../../common/shutdown_duration.h"
#include "../../common/strings.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
	enum class ScheduleKind { Shutdown, Restart, Rolling };

	// Talks to the world server running out of this directory through a request file it checks every
	// few seconds, so it works without telnet and from any shell, cron job or deploy script.
	[[noreturn]] void SendWorldShutdownRequest(const std::string &request)
	{
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

	// Shared argument handling for world:shutdown, world:restart and world:rolling-restart.
	void RunScheduleCommand(int argc, char **argv, ScheduleKind kind)
	{
		const char *name = kind == ScheduleKind::Shutdown ? "world:shutdown" :
						   kind == ScheduleKind::Restart  ? "world:restart" : "world:rolling-restart";
		const char *noun = kind == ScheduleKind::Shutdown ? "shutdown" :
						   kind == ScheduleKind::Restart  ? "restart" : "rolling zone restart";
		const char *now  = kind == ScheduleKind::Shutdown ? "shut down immediately" :
						   kind == ScheduleKind::Restart  ? "restart immediately" : "start restarting zones immediately";

		auto usage = [&]() {
			std::cout
				<< fmt::format("Usage: world {} <when>    schedule a {}\n", name, noun)
				<< fmt::format("       world {} now       {}\n", name, now)
				<< fmt::format("       world {} cancel    cancel a scheduled {}\n", name, noun)
				<< fmt::format("       world {} status    show the current schedule\n\n", name)
				<< "  when: a delay:              90m, 1h30m, 2d4h (a bare number is seconds)\n"
				<< "        a time in Eastern:    7am, 7:30pm, 19:00 (the next time it comes around)\n"
				<< "        a date and time:      10/5/2026 7am, \"Oct 5 2026 7:00am\", 2026-10-05 07:00\n"
				<< "                              (Eastern unless it ends in UTC)\n"
				<< "        a Unix timestamp:     1759662000\n"
				<< "  Warnings: MOTD only until 60 minutes remain, then every 5 minutes, then every minute from 15 minutes.\n"
				<< "  Run from the server directory (where eqemu_config.json is).\n";
			std::exit(1);
		};

		std::vector<std::string> args;
		for (int i = 2; i < argc; i++) {
			args.emplace_back(argv[i]);
		}

		if (args.empty()) {
			usage();
		}

		const char *user = std::getenv("USER");
		if (!user) {
			user = std::getenv("USERNAME");
		}
		const std::string by = fmt::format("command line ({})", user ? user : "unknown");

		// Request verbs understood by WorldShutdown::HandleRequest.
		const char *schedule_verb = kind == ScheduleKind::Shutdown ? "schedule" : kind == ScheduleKind::Restart ? "restart" : "rolling";
		const char *now_verb      = kind == ScheduleKind::Shutdown ? "now" : kind == ScheduleKind::Restart ? "restartnow" : "rollingnow";
		// A world restart shares cancel/status with the shutdown schedule.
		const char *cancel_verb   = kind == ScheduleKind::Rolling ? "rollingcancel" : "cancel";
		const char *status_verb   = kind == ScheduleKind::Rolling ? "rollingstatus" : "status";

		const auto sub = Strings::ToLower(args[0]);
		std::string request;
		if (sub == "now") {
			request = fmt::format("{} {}", now_verb, by);
		}
		else if (sub == "cancel" || sub == "disable") {
			request = fmt::format("{} {}", cancel_verb, by);
		}
		else if (sub == "status") {
			request = status_verb;
		}
		else {
			std::string when;
			for (const auto &a : args) {
				when += (when.empty() ? "" : " ") + a;
			}

			const time_t now_time = std::time(nullptr);
			uint32       seconds  = 0;
			std::string  error;
			if (!ShutdownDuration::ParseWhen(when, now_time, seconds, error)) {
				std::cout << error << "\n\n";
				usage();
			}

			// World gets the deadline, not the delay, so the wait for it to pick this up doesn't shift it.
			std::cout << "Scheduling for " << ShutdownDuration::Eastern::Format(now_time + seconds) << "\n";
			request = fmt::format("{} {} {}", schedule_verb, static_cast<int64>(now_time + seconds), by);
		}

		SendWorldShutdownRequest(request);
	}
}

void WorldserverCLI::WorldShutdownCommand(int argc, char **argv, argh::parser &cmd, std::string &description)
{
	description = "Schedules, cancels or checks a shutdown of the running world, its zones and launcher";

	if (cmd[{"-h", "--help"}]) {
		return;
	}

	RunScheduleCommand(argc, argv, ScheduleKind::Shutdown);
}

void WorldserverCLI::WorldRestartCommand(int argc, char **argv, argh::parser &cmd, std::string &description)
{
	description = "Schedules, cancels or checks a full restart: shutdown, all zones down, then back up";

	if (cmd[{"-h", "--help"}]) {
		return;
	}

	RunScheduleCommand(argc, argv, ScheduleKind::Restart);
}

void WorldserverCLI::RollingRestartCommand(int argc, char **argv, argh::parser &cmd, std::string &description)
{
	description = "Schedules, cancels or checks a rolling restart of zones while world stays up";

	if (cmd[{"-h", "--help"}]) {
		return;
	}

	RunScheduleCommand(argc, argv, ScheduleKind::Rolling);
}
