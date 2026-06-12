// sparktest — modern speedtest client with a big live figure and a sparkline
// trend of the recent instantaneous samples. Built on the shared tui.hpp
// helpers. Pure ANSI, no extra dependencies.

#include <iostream>
#include <iomanip>
#include <csignal>
#include <algorithm>

#include "speedtest/speedtest.hpp"
#include "tui.hpp"

namespace {

	constexpr std::size_t SPARK_WIDTH = 40;

	void restore_cursor(int) {
		std::cout << tui::ansi::show_cursor << std::flush;
		std::_Exit(130);
	}

	struct View {

		bool                started = false;
		std::deque<double>  dl_hist, ul_hist;
		double              dl_max = 0.0, ul_max = 0.0;
		speedtest::Speed    dl, ul;
		bool                dl_done = false, ul_done = false;

		static void push(std::deque<double>& h, double v) {
			h.push_back(v);
			while ( h.size() > SPARK_WIDTH )
				h.pop_front();
		}

		void block(const char* arrow, const char* label, speedtest::Speed s,
		        const std::deque<double>& hist, double maxv, bool active, bool done, bool seen) {

			const char* color = done ? tui::ansi::green : ( active ? tui::ansi::cyan : tui::ansi::dim );

			std::cout << tui::ansi::clear_line << "  " << color << arrow << " "
			          << std::left << std::setw(9) << label << tui::ansi::reset;

			if ( !seen ) std::cout << tui::ansi::dim << "—" << tui::ansi::reset;
			else         std::cout << tui::ansi::bold << ( done ? tui::ansi::green : "" )
			                       << tui::mbit(s) << tui::ansi::reset;
			std::cout << "\n";

			std::cout << tui::ansi::clear_line << "    " << color
			          << tui::sparkline(hist, maxv) << tui::ansi::reset << "\n";
		}

		void draw(tui::Phase ph) {

			if ( started )
				std::cout << tui::ansi::up(5);
			started = true;

			block("↓", "DOWNLOAD", dl, dl_hist, dl_max,
			      ph == tui::Phase::Download && !dl_done, dl_done,
			      dl_done || ph == tui::Phase::Download);

			std::cout << tui::ansi::clear_line << "\n";

			block("↑", "UPLOAD", ul, ul_hist, ul_max,
			      ph == tui::Phase::Upload && !ul_done, ul_done,
			      ul_done || ph == tui::Phase::Upload);

			std::cout << std::flush;
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
		          << tui::ansi::dim << "  " << server.name << " · "
		          << std::fixed << std::setprecision(0) << server.distance << " km"
		          << " · ping " << latency << " ms · jitter " << jitter << " ms"
		          << tui::ansi::reset << "\n\n" << tui::ansi::hide_cursor << std::flush;
	};

	hooks.on_live = [&view](tui::Phase ph, speedtest::Speed instant, double, double) {
		if ( ph == tui::Phase::Download ) {
			view.dl = instant;
			view.dl_max = std::max(view.dl_max, instant.mbps());
			View::push(view.dl_hist, instant.mbps());
		} else {
			view.ul = instant;
			view.ul_max = std::max(view.ul_max, instant.mbps());
			View::push(view.ul_hist, instant.mbps());
		}
		view.draw(ph);
	};

	hooks.on_done = [&view](tui::Phase ph, speedtest::Speed result) {
		if ( ph == tui::Phase::Download ) { view.dl = result; view.dl_done = true; }
		else                              { view.ul = result; view.ul_done = true; }
		view.draw(ph);
	};

	int rc = tui::run(sp, opts, hooks);

	std::cout << tui::ansi::show_cursor;

	if ( rc != 0 )
		std::cerr << "\n" << tui::ansi::yellow << "Test failed." << tui::ansi::reset << "\n";

	return rc;
}
