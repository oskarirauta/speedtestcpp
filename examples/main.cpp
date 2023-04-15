#include <iostream>
#include <iomanip>
#include <csignal>
#include <map>

#include "speedtest/speedtest.hpp"
#include "CmdOptions.hpp"

void banner() {

	std::cout << "SpeedTest++ version " << speedtest::version << std::endl;
	std::cout << "Speedtest.net command line interface" << std::endl;
	std::cout << "Info: https://github.com/oskarirauta/SpeedTest (forked from: https://github.com/taganaka/SpeedTest)" << std::endl;
	std::cout << "Author: Francesco Laurita <francesco.laurita@gmail.com>" << std::endl;
	std::cout << "Co-authored: Oskari Rauta <oskari.rauta@gmail.com>" << std::endl;
}

void usage(const char* name) {

	std::cerr << "Usage: " << name << " " <<
		" [--latency] [--quality] [--download] [--upload] [--share] [--help]\n"
		"      [--test-server host:port] [--output verbose|text|json]\n" <<
		"\noptional arguments:\n" <<
		"  --help                      Show this message and exit\n" <<
		"  --latency                   Perform latency test only\n" <<
		"  --download                  Perform download test only. It includes latency test\n" <<
		"  --upload                    Perform upload test only. It includes latency test\n" <<
		"  --share                     Generate and provide a URL to the speedtest.net share results image\n" <<
		"  --insecure                  Skip SSL certificate verify (Useful for Embedded devices)\n" <<
		"  --test-server host:port     Run speed test against a specific server\n" <<
		"  --force-by-latency-test     Always select server based on local latency test\n" <<
		"  --output verbose|text|json  Set output type. Default: verbose\n" << std::endl;
}

int main(const int argc, const char **argv) {

	ProgramOptions programOptions;

	if ( !ParseOptions(argc, argv, programOptions)) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if ( programOptions.output_type == OutputType::verbose ) {
		banner();
		std::cout << std::endl;
	}

	if ( programOptions.help ) {
		usage(argv[0]);
		return EXIT_SUCCESS;
	}

	signal(SIGPIPE, SIG_IGN);

	auto sp = speedtest::SpeedTest(speedtest::MIN_SERVER_VERSION);
	speedtest::IPInfo info;
	speedtest::Server server;

	if ( programOptions.insecure )
		sp.set_insecure(programOptions.insecure);

	if ( !sp.ipinfo(info)) {

		if ( programOptions.output_type == OutputType::json )
			std::cout << "{\"error\":\"unable to retrieve your ip info\"}" << std::endl;
		else std::cerr << "Unable to retrieve your IP info. Try again later" << std::endl;

		return EXIT_FAILURE;

	} else if ( programOptions.output_type == OutputType::json ) std::cout << "{";

	if ( programOptions.output_type == OutputType::verbose ) {
		std::cout << "IP: " << info.ip_address <<
			" ( " << info.isp << " ) " <<
			"Location: [" << info.lat << ", " << info.lon << "]" << std::endl;
	} else if ( programOptions.output_type == OutputType::text ) {
		std::cout << "IP=" << info.ip_address << "\n" <<
			"IP_LAT=" << info.lat << "\n" <<
			"IP_LON=" << info.lon << "\n" <<
			"PROVIDER=" << info.isp << std::endl;
	} else if ( programOptions.output_type == OutputType::json ) {
		std::cout << "\"client\":{" <<
				"\"ip\":\""  << info.ip_address << "\"," <<
				"\"lat\":\"" << info.lat << "\"," <<
				"\"lon\":\"" << info.lon << "\"," <<
				"\"isp\":\"" << info.isp << "\"" <<
				"},";
	}

	auto servers = sp.servers();

	if ( programOptions.selected_server.empty()) {

		if ( programOptions.output_type == OutputType::verbose )
			std::cout << "Finding fastest server... " << std::flush;

		if ( servers.empty()) {

			if ( programOptions.output_type == OutputType::json )
				std::cout << "\"error\":\"unable to download server list\"}" << std::endl;
			else std::cerr << "Unable to download server list. Try again later" << std::endl;

			return EXIT_FAILURE;
		}

		if ( programOptions.output_type == OutputType::verbose )
			std::cout << servers.size() << " Servers online" << std::endl;
		else if ( programOptions.output_type == OutputType::json )
			std::cout << "\"servers_online\":\"" << servers.size() << "\",";

		bool recommended_chosen = false;

		if ( !programOptions.force_ping_selected )
			recommended_chosen = sp.select_recommended_server(server);

		if ( !recommended_chosen )
			server = sp.best_server(10, [&programOptions](bool success, const speedtest::Server& server, long ms) {
				if (programOptions.output_type == OutputType::verbose)
					std::cout << (success ? '.' : '*') << std::flush;
			});

		if ( programOptions.output_type == OutputType::verbose ) {

			std::cout << std::endl;
			std::cout << "Server: " << server.name <<
				" " << server.host <<
				" by " << server.sponsor <<
				" (" << server.distance << " km from you): " <<
				sp.latency() << " ms" <<
				( recommended_chosen ? " (chosen by recommendation)" :
					( server.recommended ? " (recommended by server)" : "" )) << std::endl;

		} else if ( programOptions.output_type == OutputType::text ) {

			std::cout << "TEST_SERVER_HOST=" << server.host << std::endl;
			std::cout << "TEST_SERVER_DISTANCE=" << server.distance << std::endl;

		} else if ( programOptions.output_type == OutputType::json ) {

			std::cout << "\"server\":{" <<
				"\"name\":\"" << server.name << "\"," <<
				"\"sponsor\":\"" << server.sponsor << "\"," <<
				"\"distance\":\"" << server.distance << "\"," <<
				"\"latency\":\"" << sp.latency() << "\"," <<
				"\"host\":\"" << server.host << "\"," <<
				"\"recommended\":" << ( server.recommended ? "1" : "0" ) << "},";
		}

	} else {

		server.host.append(programOptions.selected_server);
		sp.set_server(server);

		for ( auto &s : servers )
			if ( s.host == server.host )
				server.id = s.id;

		if ( programOptions.output_type == OutputType::verbose )
			std::cout << "Selected server: " << server.host << std::endl;
		else if ( programOptions.output_type == OutputType::text )
			std::cout << "TEST_SERVER_HOST=" << server.host << std::endl;
		else if ( programOptions.output_type == OutputType::json )
			std::cout << "\"server\":{\"host\":\"" << server.host << "\"},";
	}

	if ( programOptions.output_type == OutputType::verbose )
		std::cout << "Ping: " << sp.latency() << " ms." << std::endl;
	else if ( programOptions.output_type == OutputType::text )
		std::cout << "LATENCY=" << sp.latency() << std::endl;
	else if ( programOptions.output_type == OutputType::json )
		std::cout << "\"ping\":\"" << std::fixed << sp.latency() << "\",";

	long jitter = 0;

	if ( programOptions.output_type == OutputType::verbose )
		std::cout << "Jitter: " << std::flush;

	if ( sp.jitter(server, jitter)) {

		if ( programOptions.output_type == OutputType::verbose )
			std::cout << jitter << " ms." << std::endl;
		else if ( programOptions.output_type == OutputType::text )
			std::cout << "JITTER=" << jitter << std::endl;
		else if ( programOptions.output_type == OutputType::json )
			std::cout << "\"jitter\":\"" << std::fixed << jitter << "\",";
	} else {
		if ( programOptions.output_type == OutputType::json )
			std::cout << "\"jitter\":\"-1\",";
		else std::cerr << "Jitter measurement is unavailable at this time." << std::endl;
	}

	if ( programOptions.latency ) {

		if ( programOptions.output_type == OutputType::json )
			std::cout << "\"_\":\"only latency requested\"}" << std::endl;

		return EXIT_SUCCESS;
	}

	if ( programOptions.output_type == OutputType::verbose )
		std::cout << "Determine line type (" << speedtest::Config::preflight.concurrency << ") " << std::flush;

	double preSpeed = 0;

	if ( !sp.download_speed(server, speedtest::Config::preflight, preSpeed, [&programOptions](bool success, double current_speed) {
		if ( programOptions.output_type == OutputType::verbose )
		std::cout << ( success ? '.' : '*' ) << std::flush;
	})) {

		if ( programOptions.output_type == OutputType::json )
			std::cout << "\"error\":\"pre-flight check failed\"}" << std::endl;
		else std::cerr << "Pre-flight check failed." << std::endl;

		return EXIT_FAILURE;
	}

	if ( programOptions.output_type == OutputType::verbose )
		std::cout << std::endl;

	speedtest::Profile profile(preSpeed);

	if ( programOptions.output_type == OutputType::verbose )
		std::cout << profile.description << " detected: profile selected " << profile.name << std::endl;

	if ( !programOptions.upload ) {

		if ( programOptions.output_type == OutputType::verbose ) {
			std::cout << std::endl;
			std::cout << "Testing download speed (" << profile.download.concurrency << ") "  << std::flush;
		}

		double downloadSpeed = 0;

		if ( sp.download_speed(server, profile.download, downloadSpeed, [&programOptions](bool success, long current_speed) {

			if ( programOptions.output_type == OutputType::verbose )
				std::cout << (success ? '.' : '*') << std::flush;

		})) {

			if ( programOptions.output_type == OutputType::verbose )
				std::cout << "\nDownload: " <<
					std::fixed << std::setprecision(2) <<
					downloadSpeed << " Mbit/s" << std::endl;
			else if ( programOptions.output_type == OutputType::text )
				std::cout << "DOWNLOAD_SPEED=" <<
					std::fixed << std::setprecision(2) <<
					downloadSpeed << std::endl;
			else if ( programOptions.output_type == OutputType::json )
				std::cout << "\"download\":\"" <<
					std::fixed << ( downloadSpeed * 1000 * 1000 ) << "\",";

		} else {

			if ( programOptions.output_type == OutputType::json )
				std::cout << "\"error\":\"download test failed\"}" << std::endl;
			else std::cerr << "Download test failed." << std::endl;

			return EXIT_FAILURE;
		}
	}

	if ( programOptions.download ) {

		if ( programOptions.output_type == OutputType::json )
			std::cout << "\"_\":\"only download requested\"}" << std::endl;

		return EXIT_SUCCESS;
	}

	if ( programOptions.output_type == OutputType::verbose )
		std::cout << "Testing upload speed (" << profile.upload.concurrency << ") " << std::flush;

	double uploadSpeed = 0;

	if ( sp.upload_speed(server, profile.upload, uploadSpeed, [&programOptions](bool success, double current_speed) {

		if ( programOptions.output_type == OutputType::verbose )
			std::cout << ( success ? '.' : '*' ) << std::flush;
	})) {

		if ( programOptions.output_type == OutputType::verbose )
			std::cout << "\nUpload: " <<
				std::fixed << std::setprecision(2) << uploadSpeed << " Mbit/s" << std::endl;
		else if ( programOptions.output_type == OutputType::text )
			std::cout << "UPLOAD_SPEED=" <<
				std::fixed << std::setprecision(2) << uploadSpeed << std::endl;
		else if ( programOptions.output_type == OutputType::json )
			std::cout << "\"upload\":\"" << std::fixed << ( uploadSpeed * 1000 * 1000 ) << "\",";

	} else {

		if ( programOptions.output_type == OutputType::json )
			std::cout << "\"error\":\"upload test failed\"}" << std::endl;
		else std::cerr << "Upload test failed." << std::endl;

		return EXIT_FAILURE;
	}


	if ( programOptions.share ) {

		std::string share_it;

		if ( sp.share(server, share_it)) {

			if ( programOptions.output_type == OutputType::verbose )
				std::cout << "Results image: " << share_it << std::endl;
			else if ( programOptions.output_type == OutputType::text )
				std::cout << "IMAGE_URL=" << share_it << std::endl;
			else if ( programOptions.output_type == OutputType::json )
				std::cout << "\"share\":\"" << share_it << "\",";
		} else {
			if ( programOptions.output_type == OutputType::json )
				std::cout << "\"share\":\"\",\"error\":\"shareable result image generation failed\",";
			else std::cerr << "Failed to generate shareable result image for results" << std::endl;
		}
	}

	if ( programOptions.output_type == OutputType::json )
		std::cout << "\"_\":\"all ok\"}" << std::endl;

	return EXIT_SUCCESS;
}
