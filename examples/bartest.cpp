// bartest — modern speedtest client with live filling bar meters.
// Demonstrates building a richer UI on top of the speedtest library using the
// shared helpers in tui.hpp. Pure ANSI, no extra dependencies.

#include <iostream>
#include <iomanip>
#include <csignal>
#include <algorithm>

#include "speedtest/speedtest.hpp"
#include "tui.hpp"

namespace {

	constexpr int METER_WIDTH = 24;
	constexpr int PROG_WIDTH  = 20;

	void restore_cursor(int) {
		std::cout << tui::ansi::show_cursor << std::flush;
		std::_Exit(130);
	}

	struct View {

		bool             started = false;
		double           peak    = 0.0;   // highest instantaneous speed seen
		double           total   = 20.0;  // phase duration in seconds
		speedtest::Speed dl, ul;
		bool             dl_done = false, ul_done = false;

		void line_meter(const char* label, speedtest::Speed s, bool active, bool done, bool seen) {

			double frac = peak > 0.0 ? s.mbps() / peak : 0.0;

			std::cout << tui::ansi::clear_line << "  " << std::left << std::setw(9) << label
			          << tui::ansi::dim << "▕" << tui::ansi::reset;

			std::cout << ( done ? tui::ansi::green : ( active ? tui::ansi::cyan : tui::ansi::dim ))
			          << tui::bar(seen ? frac : 0.0, METER_WIDTH) << tui::ansi::reset
			          << tui::ansi::dim << "▏" << tui::ansi::reset << "  ";

			if ( !seen )      std::cout << tui::ansi::dim << "—" << tui::ansi::reset;
			else if ( done )  std::cout << tui::ansi::bold << tui::ansi::green << tui::mbit(s) << tui::ansi::reset;
			else              std::cout << tui::ansi::bold << tui::mbit(s) << tui::ansi::reset;

			std::cout << "\n";
		}

		void draw(tui::Phase ph, double elapsed, double total) {

			if ( started )
				std::cout << tui::ansi::up(3);
			started = true;

			line_meter("Download", dl, ph == tui::Phase::Download && !dl_done, dl_done, dl_done || ph == tui::Phase::Download);
			line_meter("Upload",   ul, ph == tui::Phase::Upload   && !ul_done, ul_done, ul_done || ph == tui::Phase::Upload);

			double frac = total > 0.0 ? std::min(elapsed / total, 1.0) : 0.0;
			std::cout << tui::ansi::clear_line << "  " << tui::ansi::dim
			          << ( ph == tui::Phase::Download ? "down " : "up   " )
			          << tui::bar(frac, PROG_WIDTH) << " "
			          << std::fixed << std::setprecision(1) << elapsed << " / "
			          << std::setprecision(1) << total << " s"
			          << tui::ansi::reset << "\n" << std::flush;
		}
	};
}

int main(int argc, const char** argv) {

	signal(SIGINT, restore_cursor);
	signal(SIGPIPE, SIG_IGN);

	tui::Options opts = tui::parse(argc, argv);

	auto sp = speedtest::SpeedTest();
	if ( opts.insecure )
		sp.set_insecure(true);

	View view;

	tui::Hooks hooks;

	hooks.on_stage = [](const std::string& s) {
		std::cout << "\r" << tui::ansi::clear_line << tui::ansi::dim
		          << s << " ..." << tui::ansi::reset << std::flush;
	};

	hooks.on_ready = [](const speedtest::Server& server, const speedtest::IPInfo& info,
	        long latency, long jitter) {
		std::cout << "\r" << tui::ansi::clear_line
		          << tui::ansi::bold << "SpeedTest++ " << speedtest::version << tui::ansi::reset
		          << tui::ansi::dim << "  (" << info.isp << ")" << tui::ansi::reset << "\n"
		          << tui::ansi::dim << "  " << server.name << " · " << server.sponsor
		          << " · " << std::fixed << std::setprecision(0) << server.distance << " km"
		          << " · ping " << latency << " ms · jitter " << jitter << " ms"
		          << tui::ansi::reset << "\n\n" << tui::ansi::hide_cursor << std::flush;
	};

	hooks.on_live = [&view](tui::Phase ph, speedtest::Speed instant, double el, double total) {
		view.peak  = std::max(view.peak, instant.mbps());
		view.total = total;
		if ( ph == tui::Phase::Download ) view.dl = instant;
		else                              view.ul = instant;
		view.draw(ph, el, total);
	};

	hooks.on_done = [&view](tui::Phase ph, speedtest::Speed result) {
		view.peak = std::max(view.peak, result.mbps());
		if ( ph == tui::Phase::Download ) { view.dl = result; view.dl_done = true; }
		else                              { view.ul = result; view.ul_done = true; }
		view.draw(ph, view.total, view.total);
	};

	int rc = tui::run(sp, opts, hooks);

	std::cout << tui::ansi::show_cursor;

	if ( rc != 0 )
		std::cerr << "\n" << tui::ansi::yellow << "Test failed." << tui::ansi::reset << "\n";

	return rc;
}
