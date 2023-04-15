#include <iostream>
#include <iomanip>
#include <csignal>
#include <thread>
#include <mutex>

#include "speedtest/speedtest.hpp"

int main(const int argc, const char **argv) {

	std::cout << "SpeedTest++ mini version " << speedtest::version << std::endl;

	if ( !speedtest::git_commit.empty())
		std::cout << "git commit: " + speedtest::git_commit << std::endl;

	std::cout << "Minimal Speedtest.net command line interface example" << std::endl;
	std::cout << "Author: Oskari Rauta <oskari.rauta@gmail.com>" << std::endl;

	std::cout << "\nPlease wait patiently. Test is in progress.\n" << std::endl;

	signal(SIGPIPE, SIG_IGN);

	speedtest::SpeedTest sp;
	speedtest::IPInfo info;

	if ( !sp.ipinfo(info)) {

		std::cerr << "error: unable to retrieve your IP info" << std::endl;
		return -1;
	}

	std::cout << " - IP address information retrieved" << std::endl;

	speedtest::Server server;

	if ( !sp.select_recommended_server(server))
		server = sp.best_server(10, nullptr);

	long latency = sp.latency();
	std::cout << " - Latency test finished" << std::endl;

	long jitter = 0;

	if ( !sp.jitter(server, jitter)) {
		std::cout << " - Jitter test skipped, unavailable" << std::endl;
		jitter = -1;
	} else std::cout << " - Jitter test finished" << std::endl;

	double dl = 0, ul = 0;
	double previous_speed = -1;
	std::mutex output_mutex;

	std::cout << "\e[?25l";
	if ( !sp.download_speed(server, speedtest::Config::preflight, dl, [&previous_speed, &output_mutex](bool success, double current_speed) {

		if ( success ) {
			output_mutex.lock();
			std::cout << ( previous_speed == -1 ? " - Pre-flight check: " : "\r - Pre-flight check: " ) <<
				std::setprecision(2) << ( current_speed / 1000 / 1000 * speedtest::Config::preflight.concurrency ) <<
				" Mbit/s" << "          " << std::flush;
			previous_speed = current_speed;
			output_mutex.unlock();
		}

	})) {
		std::cout << "\e[?25h";
		std::cout << ( previous_speed == -1 ? " - Pre-flight check: failure" : "\r - Pre-flight check: failure" ) <<
			"          " << std::endl;
		std::cerr << "error: pre-flight check failed" << std::endl;
		return -1;
	} else std::cout << "\r - Pre-flight check finished" << "          " << std::endl;

	speedtest::Profile profile(dl);
	dl = 0;
	previous_speed = -1;

	if ( !sp.download_speed(server, profile.download, dl, [&previous_speed, &profile, &output_mutex](bool success, double current_speed) {

		if ( success ) {
			output_mutex.lock();
			std::cout << ( previous_speed == -1 ? " - Download test: " : "\r - Download test: " ) <<
				std::setprecision(2) <<
				(double)(current_speed / 1000 / 1000 * profile.download.concurrency ) <<
				" Mbit/s" << "          " << std::flush;
			previous_speed = current_speed;
			output_mutex.unlock();
		}

	})) {
		std::cout << "\e[?25h";
		std::cout << ( previous_speed == -1 ? " - Download test: failure" : "\r - Download test: failure" ) <<
			"          " << std::endl;
		std::cerr << "error: download test failed" << std::endl;
		return -1;
	} else std::cout << "\r - Download test finished" << "          " << std::endl;

	previous_speed = -1;

	if ( !sp.upload_speed(server, profile.upload, ul, [&previous_speed, &profile, &output_mutex](bool success, double current_speed) {

		if ( success ) {
			output_mutex.lock();
			std::cout << ( previous_speed == -1 ? " - Upload test: " : "\r - Upload test: " ) <<
				std::setprecision(2) <<
				( current_speed / 1000 / 1000 * profile.upload.concurrency ) <<
				" Mbit/s" << "          " << std::flush;
			previous_speed = current_speed;
			output_mutex.unlock();
		}

	})) {
		std::cout << "\e[?25h";
		std::cout << ( previous_speed == -1 ? " - Upload test: failure" : "\r - Upload test: failure" ) <<
			"          " << std::endl;
		std::cerr << "error: upload test failed" << std::endl;
		return -1;
	} else std::cout << "\r - Upload test finished" << "          " << std::endl;

	std::cout << "\e[?25h";
	std::cout << "\nResults:\n" <<
		"Server: " << server.host << " by " << server.sponsor << " (" << server.distance << " km" <<
		( server.recommended ? ", recommended by list" : "" ) << ")\n" <<
		"Ping: " << latency << " ms.\n" <<
		"Jitter: " << ( jitter == -1 ? "unavailable\n" : ( std::to_string(jitter) + "ms.\n")) <<
		"Profile: " << profile.name << "\n" <<
		"Download speed: " << std::fixed << std::setprecision(2) << dl << " Mbit/s\n" <<
		"Upload speed: " << std::fixed << std::setprecision(2) << ul << " Mbit/s\n" << std::endl;

	return 0;
}
