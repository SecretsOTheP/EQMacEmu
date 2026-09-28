#include "global_define.h"
#include "eqemu_logsys.h"
#include "crash.h"
#include "strings.h"
#include "process/process.h"
#include "http/httplib.h"
#include "http/uri.h"
#include "json/json.h"
#include "version.h"
#include "eqemu_config.h"
#include "serverinfo.h"
#include "rulesys.h"
#include "platform.h"

#include <cstdio>
#include <vector>

#ifdef _WINDOWS
#define popen _popen
#endif

void SendCrashReport(const std::string& crash_report)
{
	// can configure multiple endpoints if need be
	std::vector<std::string> endpoints = {
		"https://spire.akkadius.com/api/v1/analytics/server-crash-report",
		//		"http://localhost:3010/api/v1/analytics/server-crash-report", // development
	};

	auto      config = EQEmuConfig::get();
	for (auto& e : endpoints) {
		uri u(e);

		std::string base_url = fmt::format("{}://{}", u.get_scheme(), u.get_host());
		if (u.get_port()) {
			base_url += fmt::format(":{}", u.get_port());
		}

		// client
		httplib::Client r(base_url);
		r.set_connection_timeout(1, 0);
		r.set_read_timeout(1, 0);
		r.set_write_timeout(1, 0);

		// os info
		auto os = EQ::GetOS();
		auto cpus = EQ::GetCPUs();
		auto process_id = EQ::GetPID();
		auto rss = EQ::GetRSS() / 1048576.0;
		auto uptime = static_cast<uint32>(EQ::GetUptime());

		// payload
		Json::Value p;
		p["platform_name"] = GetPlatformName();
		p["crash_report"] = crash_report;
		p["server_version"] = CURRENT_VERSION;
		p["compile_date"] = COMPILE_DATE;
		p["compile_time"] = COMPILE_TIME;
		p["server_name"] = config->LongName;
		p["server_short_name"] = config->ShortName;
		p["uptime"] = uptime;
		p["os_machine"] = os.machine;
		p["os_release"] = os.release;
		p["os_version"] = os.version;
		p["os_sysname"] = os.sysname;
		p["process_id"] = process_id;
		p["rss_memory"] = rss;
		p["cpus"] = cpus.size();
		p["origination_info"] = "";

		if (!LogSys.origination_info.zone_short_name.empty()) {
			p["origination_info"] = fmt::format(
				"{} ({})",
				LogSys.origination_info.zone_short_name,
				LogSys.origination_info.zone_long_name
			);
		}

		std::stringstream payload;
		payload << p;

		if (auto res = r.Post(e, payload.str(), "application/json")) {
			if (res->status == 200) {
				LogInfo("Sent crash report");
			}
			else {
				LogError("Failed to send crash report to [{}]", e);
			}
		}
	}
}

#if defined(_WINDOWS) && defined(CRASH_LOGGING)
#include "StackWalker.h"

class EQEmuStackWalker : public StackWalker
{
public:
	EQEmuStackWalker() : StackWalker() { }
	EQEmuStackWalker(DWORD dwProcessId, HANDLE hProcess) : StackWalker(dwProcessId, hProcess) { }
	virtual void OnOutput(LPCSTR szText) {
		char buffer[4096];
		for (int i = 0; i < 4096; ++i) {
			if (szText[i] == 0) {
				buffer[i] = '\0';
				break;
			}

			if (szText[i] == '\n' || szText[i] == '\r') {
				buffer[i] = ' ';
			}
			else {
				buffer[i] = szText[i];
			}
		}

		std::string line = buffer;
		_lines.push_back(line);

		Log(Logs::General, Logs::Crash, buffer);
		StackWalker::OnOutput(szText);
	}

	const std::vector<std::string>& GetLines() { return _lines; }
private:
	std::vector<std::string> _lines;
};

LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* ExceptionInfo)
{
	switch (ExceptionInfo->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		Log(Logs::General, Logs::Crash, "EXCEPTION_ACCESS_VIOLATION");
		break;
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		Log(Logs::General, Logs::Crash, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED");
		break;
	case EXCEPTION_BREAKPOINT:
		Log(Logs::General, Logs::Crash, "EXCEPTION_BREAKPOINT");
		break;
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		Log(Logs::General, Logs::Crash, "EXCEPTION_DATATYPE_MISALIGNMENT");
		break;
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		Log(Logs::General, Logs::Crash, "EXCEPTION_FLT_DENORMAL_OPERAND");
		break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		Log(Logs::General, Logs::Crash, "EXCEPTION_FLT_DIVIDE_BY_ZERO");
		break;
	case EXCEPTION_FLT_INEXACT_RESULT:
		Log(Logs::General, Logs::Crash, "EXCEPTION_FLT_INEXACT_RESULT");
		break;
	case EXCEPTION_FLT_INVALID_OPERATION:
		Log(Logs::General, Logs::Crash, "EXCEPTION_FLT_INVALID_OPERATION");
		break;
	case EXCEPTION_FLT_OVERFLOW:
		Log(Logs::General, Logs::Crash, "EXCEPTION_FLT_OVERFLOW");
		break;
	case EXCEPTION_FLT_STACK_CHECK:
		Log(Logs::General, Logs::Crash, "EXCEPTION_FLT_STACK_CHECK");
		break;
	case EXCEPTION_FLT_UNDERFLOW:
		Log(Logs::General, Logs::Crash, "EXCEPTION_FLT_UNDERFLOW");
		break;
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		Log(Logs::General, Logs::Crash, "EXCEPTION_ILLEGAL_INSTRUCTION");
		break;
	case EXCEPTION_IN_PAGE_ERROR:
		Log(Logs::General, Logs::Crash, "EXCEPTION_IN_PAGE_ERROR");
		break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		Log(Logs::General, Logs::Crash, "EXCEPTION_INT_DIVIDE_BY_ZERO");
		break;
	case EXCEPTION_INT_OVERFLOW:
		Log(Logs::General, Logs::Crash, "EXCEPTION_INT_OVERFLOW");
		break;
	case EXCEPTION_INVALID_DISPOSITION:
		Log(Logs::General, Logs::Crash, "EXCEPTION_INVALID_DISPOSITION");
		break;
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		Log(Logs::General, Logs::Crash, "EXCEPTION_NONCONTINUABLE_EXCEPTION");
		break;
	case EXCEPTION_PRIV_INSTRUCTION:
		Log(Logs::General, Logs::Crash, "EXCEPTION_PRIV_INSTRUCTION");
		break;
	case EXCEPTION_SINGLE_STEP:
		Log(Logs::General, Logs::Crash, "EXCEPTION_SINGLE_STEP");
		break;
	case EXCEPTION_STACK_OVERFLOW:
		Log(Logs::General, Logs::Crash, "EXCEPTION_STACK_OVERFLOW");
		break;
	default:
		Log(Logs::General, Logs::Crash, "Unknown Exception");
		break;
	}

	if (EXCEPTION_STACK_OVERFLOW != ExceptionInfo->ExceptionRecord->ExceptionCode)
	{
		EQEmuStackWalker sw;
		sw.ShowCallstack(GetCurrentThread(), ExceptionInfo->ContextRecord);

		if (RuleB(Analytics, CrashReporting)) {
			std::string crash_report;
			auto& lines = sw.GetLines();

			for (auto& line : lines) {
				crash_report += line;
				crash_report += "\n";
			}

			SendCrashReport(crash_report);
		}
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void set_exception_handler() {
	SetUnhandledExceptionFilter(windows_exception_handler);
}
#else

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <time.h>

#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <atomic>
#include <string>
#ifdef __linux__
#include <sys/prctl.h>
#endif

extern char** environ;

// The old handler ran gdb detection, logging, file streams, an HTTP upload and exit() from inside
// the signal handler. If the crash happened while that thread held the malloc, log or stdio lock
// (common with heap corruption), the handler deadlocked and the process never died, so the launcher
// never restarted it. Everything the handler needs is now prepared at startup, the handler only
// makes async-signal-safe calls, gdb runs with a hard timeout, and the process always dies with
// the original signal (the kernel still writes a core if core dumps are enabled).
namespace {
	constexpr int GDB_TIMEOUT_SECONDS = 60;

	char s_exe_path[512]     = {};
	char s_pid_str[16]       = {};
	char s_report_path[512]  = {};
	char s_gdb_path[512]     = {};
	const char* s_gdb_argv[] = { "gdb", "--batch", "-n", "-ex", "thread apply all bt", s_exe_path, s_pid_str, nullptr };

	std::atomic_flag s_handling = ATOMIC_FLAG_INIT;

	void safe_write(int fd, const char* s)
	{
		if (fd >= 0) {
			ssize_t ignored = write(fd, s, strlen(s));
			(void) ignored;
		}
	}

	void safe_write_int(int fd, long value)
	{
		char buf[24];
		int  i        = sizeof(buf) - 1;
		bool negative = value < 0;
		unsigned long v = negative ? -static_cast<unsigned long>(value) : static_cast<unsigned long>(value);
		buf[i] = '\0';
		do {
			buf[--i] = static_cast<char>('0' + (v % 10));
			v /= 10;
		} while (v && i > 1);
		if (negative) {
			buf[--i] = '-';
		}
		safe_write(fd, &buf[i]);
	}

	void find_gdb()
	{
		const char* path_env = getenv("PATH");
		std::string paths    = path_env ? path_env : "/usr/bin:/bin:/usr/local/bin";
		size_t      start    = 0;
		while (start <= paths.size()) {
			size_t      end  = paths.find(':', start);
			std::string dir  = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
			std::string full = (dir.empty() ? "." : dir) + "/gdb";
			if (access(full.c_str(), X_OK) == 0 && full.size() < sizeof(s_gdb_path)) {
				memcpy(s_gdb_path, full.c_str(), full.size() + 1);
				return;
			}
			if (end == std::string::npos) {
				break;
			}
			start = end + 1;
		}
	}

	// Runs gdb against this process and waits at most GDB_TIMEOUT_SECONDS for it.
	void run_gdb(int out_fd)
	{
		int sync_pipe[2];
		if (pipe(sync_pipe) != 0) {
			return;
		}

		// Raw clone instead of fork(): glibc's fork() runs atfork handlers that take the malloc locks,
		// which is exactly what may already be held by the crashing thread.
		pid_t child = static_cast<pid_t>(syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0));
		if (child == 0) {
			setpgid(0, 0); // own process group, so a timeout can kill gdb and anything it started
			close(sync_pipe[1]);
			char go;
			ssize_t ignored = read(sync_pipe[0], &go, 1); // wait until the parent allows us to ptrace it
			(void) ignored;
			dup2(out_fd, STDOUT_FILENO);
			dup2(out_fd, STDERR_FILENO);
			execve(s_gdb_path, const_cast<char* const*>(s_gdb_argv), environ);
			_exit(127);
		}

		close(sync_pipe[0]);
		if (child < 0) {
			close(sync_pipe[1]);
			return;
		}

#ifdef __linux__
		// With Yama ptrace_scope=1 only an ancestor may attach; allow just this child. Ignored without Yama.
		prctl(PR_SET_PTRACER, child, 0, 0, 0);
#endif
		ssize_t ignored = write(sync_pipe[1], "g", 1);
		(void) ignored;
		close(sync_pipe[1]);

		const struct timespec tick = { 0, 100 * 1000 * 1000 };
		for (int i = 0; i < GDB_TIMEOUT_SECONDS * 10; ++i) {
			int status = 0;
			if (waitpid(child, &status, WNOHANG) == child) {
				return;
			}
			nanosleep(&tick, nullptr);
		}

		safe_write(out_fd, "\n[crash handler] gdb timed out, killing it\n");
		kill(-child, SIGKILL);
		kill(child, SIGKILL);
		int status = 0;
		waitpid(child, &status, 0);
	}

	void crash_signal_handler(int sig)
	{
		// Only the first crashing thread writes the report; any other thread that faults meanwhile waits
		// here until the process is torn down.
		if (s_handling.test_and_set()) {
			for (;;) {
				pause();
			}
		}

		int fd = open(s_report_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		safe_write(fd, "Fatal signal ");
		safe_write_int(fd, sig);
		safe_write(fd, " in pid ");
		safe_write(fd, s_pid_str);
		safe_write(fd, " exe ");
		safe_write(fd, s_exe_path);
		safe_write(fd, "\n\n");

		if (s_gdb_path[0] && fd >= 0) {
			run_gdb(fd);
		}
		else {
			safe_write(fd, "gdb not found in PATH at startup; no backtrace. Install gdb for crash backtraces.\n");
		}

		if (fd >= 0) {
			close(fd);
		}

		// Die with the original signal so the launcher sees a crash and the kernel can write a core.
		struct sigaction dfl;
		memset(&dfl, 0, sizeof(dfl));
		dfl.sa_handler = SIG_DFL;
		sigemptyset(&dfl.sa_mask);
		sigaction(sig, &dfl, nullptr);

		sigset_t unblock;
		sigemptyset(&unblock);
		sigaddset(&unblock, sig);
		sigprocmask(SIG_UNBLOCK, &unblock, nullptr);
		raise(sig);

		_exit(128 + sig); // only reached if the signal somehow didn't terminate us
	}
}

void set_exception_handler()
{
	ssize_t len = readlink("/proc/self/exe", s_exe_path, sizeof(s_exe_path) - 1);
	s_exe_path[len > 0 ? len : 0] = '\0';
	snprintf(s_pid_str, sizeof(s_pid_str), "%d", getpid());

	const char* exe_name = strrchr(s_exe_path, '/');
	exe_name = exe_name ? exe_name + 1 : (s_exe_path[0] ? s_exe_path : "process");

	mkdir("logs", 0755);
	mkdir("logs/crashes", 0755);
	snprintf(s_report_path, sizeof(s_report_path), "logs/crashes/backtrace_%.200s_%s.log", exe_name, s_pid_str);

	find_gdb();

	// Room to run the handler if the main thread overflows its stack.
	static char alt_stack[64 * 1024];
	stack_t ss;
	memset(&ss, 0, sizeof(ss));
	ss.ss_sp    = alt_stack;
	ss.ss_size  = sizeof(alt_stack);
	ss.ss_flags = 0;
	sigaltstack(&ss, nullptr);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = crash_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;

	for (int sig : { SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL }) {
		sigaction(sig, &sa, nullptr);
	}
}
#endif
