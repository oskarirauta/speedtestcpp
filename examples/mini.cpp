#include <iostream>
#include <iomanip>
#include <csignal>
#include <mutex>

#include "speedtest/speedtest.hpp"

int main() {

	std::cout << "SpeedTest++ mini " << speedtest::version << "\n"
	          << "Minimal example — no options, just results.\n\n";

	signal(SIGPIPE, SIG_IGN);

	speedtest::SpeedTest sp;
	speedtest::IPInfo    info;
	speedtest::Server    server;

	if ( !sp.ipinfo(info)) {
		std::cerr << "error: unable to retrieve IP info\n";
		return EXIT_FAILURE;
	}
	std::cout << "IP: " << info.ip_address << " (" << info.isp << ")\n";

	if ( !sp.select_recommended_server(server))
		server = sp.best_server(10);

	std::cout << "Server: " << server.name << " by " << server.sponsor
	          << " (" << server.distance << " km)\n"
	          << "Latency: " << sp.latency() << " ms\n";

	long jitter = 0;
	if ( sp.jitter(server, jitter))
		std::cout << "Jitter: " << jitter << " ms\n";

	speedtest::Profile profile(speedtest::Speed{});
	speedtest::Speed   preSpeed;

	if ( !sp.profile(profile)) {
		sp.download_speed(server, speedtest::Config::preflight, preSpeed);
		profile = speedtest::Profile(preSpeed);
	}

	std::cout << "Profile: " << profile.name << "\n\n";

	std::mutex mtx;
	speedtest::Speed dl, ul;

	std::cout << "Testing download..." << std::flush;
	sp.download_speed(server, profile.download, dl, [&mtx](bool ok, speedtest::Speed s) {
		if ( ok ) { std::lock_guard lk(mtx); std::cout << "\rDownload: " << s << "     " << std::flush; }
	});
	std::cout << "\rDownload: " << dl << "                    \n";

	std::cout << "Testing upload..." << std::flush;
	sp.upload_speed(server, profile.upload, ul, [&mtx](bool ok, speedtest::Speed s) {
		if ( ok ) { std::lock_guard lk(mtx); std::cout << "\rUpload:   " << s << "     " << std::flush; }
	});
	std::cout << "\rUpload:   " << ul << "                    \n";

	return EXIT_SUCCESS;
}
