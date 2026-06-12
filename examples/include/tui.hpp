#pragma once

// Shared terminal-UI helpers and test orchestration for the modern example
// clients (bartest, sparktest). Pure ANSI, no extra dependencies. The legacy
// `speedtest` client and the minimal `minitest` demo do not use this.

#include <string>
#include <sstream>
#include <iomanip>
#include <deque>
#include <functional>
#include <chrono>
#include <cmath>

#include "speedtest/speedtest.hpp"

namespace tui {

	enum class Phase { Download, Upload };

	namespace ansi {

		inline constexpr const char* reset       = "\033[0m";
		inline constexpr const char* bold        = "\033[1m";
		inline constexpr const char* dim         = "\033[2m";
		inline constexpr const char* green       = "\033[32m";
		inline constexpr const char* cyan        = "\033[36m";
		inline constexpr const char* yellow      = "\033[33m";
		inline constexpr const char* magenta     = "\033[35m";
		inline constexpr const char* hide_cursor = "\033[?25l";
		inline constexpr const char* show_cursor = "\033[?25h";
		inline constexpr const char* clear_line  = "\033[2K";

		inline std::string up(int n) { return "\033[" + std::to_string(n) + "A"; }
	}

	// A horizontal meter; `frac` in [0,1] maps to a filled portion of `width`.
	inline std::string bar(double frac, int width) {

		if ( frac < 0.0 ) frac = 0.0;
		if ( frac > 1.0 ) frac = 1.0;

		int full = static_cast<int>(frac * width + 0.5);
		std::string s;

		for ( int i = 0; i < width; i++ )
			s += ( i < full ? "█" : "░" );

		return s;
	}

	// A unicode sparkline of the samples, scaled to `maxv`.
	inline std::string sparkline(const std::deque<double>& samples, double maxv) {

		static const char* ticks[] = {
			"▁", "▂", "▃", "▄",
			"▅", "▆", "▇", "█"
		};

		std::string s;

		for ( double x : samples ) {
			double f = maxv > 0.0 ? x / maxv : 0.0;
			if ( f < 0.0 ) f = 0.0;
			if ( f > 1.0 ) f = 1.0;
			s += ticks[static_cast<int>(f * 7 + 0.5)];
		}

		return s;
	}

	inline std::string mbit(speedtest::Speed s) {
		std::ostringstream o;
		o << std::fixed << std::setprecision(2) << s.mbps() << " Mbit/s";
		return o.str();
	}

	struct Options {
		bool        insecure = false;
		std::string server;     // optional host:port; empty = auto-select
	};

	struct Hooks {
		// Short textual stage message (e.g. while selecting a server).
		std::function<void(const std::string&)> on_stage;
		// Server chosen and latency/jitter measured.
		std::function<void(const speedtest::Server&, const speedtest::IPInfo&,
		        long latency_ms, long jitter_ms)> on_ready;
		// Live update: windowed instantaneous speed + elapsed/total seconds.
		std::function<void(Phase, speedtest::Speed instant,
		        double elapsed_s, double total_s)> on_live;
		// Phase finished; `result` is the accurate cumulative average.
		std::function<void(Phase, speedtest::Speed result)> on_done;
	};

	// Drives a full test (server selection, latency, jitter, profile, download,
	// upload), reporting progress through `hooks`. Returns 0 on success.
	inline int run(speedtest::SpeedTest& sp, const Options& opts, const Hooks& hooks) {

		using namespace std::chrono;

		speedtest::IPInfo info;
		if ( !sp.ipinfo(info))
			return 1;

		speedtest::Server server;

		if ( !opts.server.empty()) {
			if ( hooks.on_stage ) hooks.on_stage("Connecting to server");
			auto servers = sp.servers();
			server.host.append(opts.server);
			if ( !sp.set_server(server, servers))
				return 1;
		} else {
			if ( hooks.on_stage ) hooks.on_stage("Finding fastest server");
			if ( sp.servers().empty())
				return 1;
			if ( !sp.select_recommended_server(server))
				server = sp.best_server(10);
		}

		long latency = sp.latency();
		long jitter  = 0;
		sp.jitter(server, jitter);

		if ( hooks.on_ready )
			hooks.on_ready(server, info, latency, jitter);

		// Pre-flight measurement selects the line profile.
		if ( hooks.on_stage ) hooks.on_stage("Analysing line");
		speedtest::Speed pre;
		if ( !sp.download_speed(server, speedtest::Config::preflight, pre))
			return 1;
		speedtest::Profile profile(pre);

		auto phase = [&](Phase ph, const speedtest::Config& cfg, bool upload) -> bool {

			double total = cfg.min_test_time_ms / 1000.0;
			auto   start = steady_clock::now();
			speedtest::Speed result;

			auto cb = [&](bool success, speedtest::Speed instant) {
				if ( !success ) return;
				double el = duration_cast<microseconds>(steady_clock::now() - start).count() / 1e6;
				if ( hooks.on_live ) hooks.on_live(ph, instant, el, total);
			};

			bool ok = upload ? sp.upload_speed(server, cfg, result, cb)
			                 : sp.download_speed(server, cfg, result, cb);

			if ( ok && hooks.on_done )
				hooks.on_done(ph, result);

			return ok;
		};

		if ( !phase(Phase::Download, profile.download, false)) return 1;
		if ( !phase(Phase::Upload,   profile.upload,   true))  return 1;

		return 0;
	}

	// Parse a tiny argv: `--insecure` and an optional host:port token.
	inline Options parse(int argc, const char** argv) {
		Options o;
		for ( int i = 1; i < argc; i++ ) {
			std::string a = argv[i];
			if ( a == "--insecure" ) o.insecure = true;
			else if ( a.find(':') != std::string::npos ) o.server = a;
		}
		return o;
	}
}
