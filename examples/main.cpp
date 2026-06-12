#include <iostream>
#include <iomanip>
#include <csignal>
#include <mutex>

#include "speedtest/speedtest.hpp"
#include "CmdOptions.hpp"

void banner() {
	std::cout << "SpeedTest++ version " << speedtest::version << "\n";
	if ( !speedtest::git_commit.empty())
		std::cout << "git commit: " << speedtest::git_commit << "\n";
	std::cout << "Speedtest.net command line interface\n";
	std::cout << "Info: https://github.com/oskarirauta/speedtestcpp\n";
	std::cout << "Author: Francesco Laurita <francesco.laurita@gmail.com>\n";
	std::cout << "Co-authored-by: Oskari Rauta <oskari.rauta@gmail.com>\n";
}

void usage(const char* name) {
	std::cerr << "Usage: " << name <<
		" [--latency] [--download] [--upload] [--share] [--help]\n"
		"      [--test-server host:port] [--output verbose|text|json]\n"
		"\noptional arguments:\n"
		"  --help                      Show this message and exit\n"
		"  --list-servers              Show list of servers\n"
		"  --latency                   Perform latency test only\n"
		"  --download                  Perform download test only. It includes latency test\n"
		"  --upload                    Perform upload test only. It includes latency test\n"
		"  --share                     Generate and provide a URL to the speedtest.net share results image\n"
		"  --insecure                  Skip SSL certificate verify (Useful for Embedded devices)\n"
		"  --test-server host:port     Run speed test against a specific server\n"
		"  --force-by-latency-test     Always select server based on local latency test\n"
		"  --output verbose|text|json  Set output type. Default: verbose\n" << std::endl;
}

int main(const int argc, const char **argv) {

	ProgramOptions opts;

	if ( !ParseOptions(argc, argv, opts)) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if ( opts.output_type == OutputType::verbose )
		banner();

	if ( opts.help ) {
		usage(argv[0]);
		return EXIT_SUCCESS;
	}

	signal(SIGPIPE, SIG_IGN);

	auto sp = speedtest::SpeedTest();
	speedtest::IPInfo info;
	speedtest::Server server;

	if ( opts.insecure )
		sp.set_insecure(true);

	if ( !sp.ipinfo(info)) {
		if ( opts.output_type == OutputType::json )
			std::cout << "{\"error\":\"unable to retrieve your ip info\"}\n";
		else
			std::cerr << "Unable to retrieve your IP info. Try again later\n";
		return EXIT_FAILURE;
	} else if ( opts.output_type == OutputType::json )
		std::cout << "{";

	if ( opts.output_type == OutputType::verbose ) {
		std::cout << "\nIP: " << info.ip_address
		          << " ( " << info.isp << " ) "
		          << "Location: [" << info.lat << ", " << info.lon << "]\n";
	} else if ( opts.output_type == OutputType::text ) {
		std::cout << "IP=" << info.ip_address << "\n"
		          << "IP_LAT=" << info.lat << "\n"
		          << "IP_LON=" << info.lon << "\n"
		          << "PROVIDER=" << info.isp << "\n";
	} else if ( opts.output_type == OutputType::json ) {
		std::cout << "\"client\":{"
		          << "\"ip\":\""   << info.ip_address << "\","
		          << "\"lat\":\"" << info.lat << "\","
		          << "\"lon\":\"" << info.lon << "\","
		          << "\"isp\":\"" << info.isp << "\"},";
	}

	auto servers = sp.servers();
	bool recommended_chosen = false;

	if ( servers.empty()) {
		if ( opts.output_type == OutputType::json )
			std::cout << "\"error\":\"unable to download server list\"}\n";
		else
			std::cerr << "Unable to download server list. Try again later\n";
		return EXIT_FAILURE;
	}

	if ( opts.list && opts.output_type != OutputType::text ) {

		if ( opts.output_type == OutputType::json )
			std::cout << "\"servers_online\":" << servers.size() << ","
			          << "\"servers\":[";
		else
			std::cout << "\n";

		int i = 0;
		for ( auto& s : servers ) {
			if ( opts.output_type == OutputType::verbose )
				std::cout << "Server #" << s.id << ": " << s.name
				          << " " << s.host
				          << " by " << s.sponsor
				          << " (" << s.distance << " km"
				          << ( s.recommended ? ", recommended)" : ")" ) << "\n";
			else
				std::cout << ( i == 0 ? "{" : ",{" )
				          << "\"id\":"         << s.id           << ","
				          << "\"host\":\""     << s.host         << "\","
				          << "\"name\":\""     << s.name         << "\","
				          << "\"sponsor\":\"" << s.sponsor       << "\","
				          << "\"url\":\""      << s.url          << "\","
				          << "\"country\":\"" << s.country       << "\","
				          << "\"country_code\":\"" << s.country_code << "\","
				          << "\"lat\":\""      << s.lat          << "\","
				          << "\"lon\":\""      << s.lon          << "\","
				          << "\"distance\":"   << s.distance     << ","
				          << "\"recommended\":" << ( s.recommended ? "1" : "0" )
				          << "}";
			i++;
		}

		if ( opts.output_type == OutputType::verbose )
			std::cout << "\n";
		else
			std::cout << "],";
	}

	if ( opts.selected_server.empty()) {

		if ( !opts.force_ping_selected )
			recommended_chosen = sp.select_recommended_server(server);

		if ( !recommended_chosen ) {

			if ( opts.output_type == OutputType::verbose )
				std::cout << "Finding fastest server...\n";

			if ( !opts.list ) {
				if ( opts.output_type == OutputType::verbose )
					std::cout << servers.size() << " Servers online\n";
				else if ( opts.output_type == OutputType::json )
					std::cout << "\"servers_online\":" << servers.size() << ",";
			}

			server = sp.best_server(10, [&opts](bool success, const speedtest::Server&, long) {
				if ( opts.output_type == OutputType::verbose )
					std::cout << ( success ? '.' : '*' ) << std::flush;
			});

			if ( opts.output_type == OutputType::verbose )
				std::cout << "\n";
		}

	} else {
		server.host.append(opts.selected_server);
		sp.set_server(server, servers);
		recommended_chosen = server.recommended;
	}

	if ( opts.output_type == OutputType::verbose ) {
		std::cout << "Server #" << server.id << ": " << server.name
		          << " " << server.host
		          << " by " << server.sponsor
		          << " (" << server.distance << " km): "
		          << sp.latency() << " ms"
		          << ( recommended_chosen ? " (chosen by recommendation)" :
		               ( server.recommended ? " (recommended by server)" : "" ))
		          << "\n";
	} else if ( opts.output_type == OutputType::text ) {
		std::cout << "TEST_SERVER_HOST=" << server.host << "\n"
		          << "TEST_SERVER_DISTANCE=" << server.distance << "\n";
	} else if ( opts.output_type == OutputType::json ) {
		std::cout << "\"server\":{"
		          << "\"name\":\""     << server.name    << "\","
		          << "\"id\":"          << server.id      << ","
		          << "\"sponsor\":\"" << server.sponsor  << "\","
		          << "\"distance\":"   << server.distance << ","
		          << "\"latency\":"    << sp.latency()    << ","
		          << "\"host\":\""     << server.host     << "\","
		          << "\"recommended\":" << ( server.recommended ? "1" : "0" ) << "},";
	}

	if ( opts.output_type == OutputType::verbose )
		std::cout << "Ping: " << sp.latency() << " ms.\n";
	else if ( opts.output_type == OutputType::text )
		std::cout << "LATENCY=" << sp.latency() << "\n";
	else if ( opts.output_type == OutputType::json )
		std::cout << "\"ping\":" << sp.latency() << ",";

	long jitter = 0;

	if ( opts.output_type == OutputType::verbose )
		std::cout << "Jitter: " << std::flush;

	if ( sp.jitter(server, jitter)) {
		if ( opts.output_type == OutputType::verbose )
			std::cout << jitter << " ms.\n";
		else if ( opts.output_type == OutputType::text )
			std::cout << "JITTER=" << jitter << "\n";
		else if ( opts.output_type == OutputType::json )
			std::cout << "\"jitter\":" << jitter << ",";
	} else {
		if ( opts.output_type == OutputType::json )
			std::cout << "\"jitter\":-1,";
		else
			std::cerr << "Jitter measurement is unavailable at this time.\n";
	}

	if ( opts.latency ) {
		if ( opts.output_type == OutputType::json )
			std::cout << "\"_\":\"only latency requested\"}\n";
		return EXIT_SUCCESS;
	}

	speedtest::Profile profile(speedtest::Speed{});

	if ( !sp.profile(profile)) {

		if ( opts.output_type == OutputType::verbose )
			std::cout << "\nDetermine line type (" << speedtest::Config::preflight.concurrency << ") " << std::flush;

		speedtest::Speed preSpeed;

		if ( !sp.download_speed(server, speedtest::Config::preflight, preSpeed,
		        [&opts](bool success, speedtest::Speed) {
			if ( opts.output_type == OutputType::verbose )
				std::cout << ( success ? '.' : '*' ) << std::flush;
		})) {
			if ( opts.output_type == OutputType::json )
				std::cout << "\"error\":\"pre-flight check failed\"}\n";
			else
				std::cerr << "Pre-flight check failed.\n";
			return EXIT_FAILURE;
		}

		profile = speedtest::Profile(preSpeed);
	}

	if ( opts.output_type == OutputType::verbose )
		std::cout << "\n" << profile.description << " detected: profile selected " << profile.name << "\n";

	std::mutex output_mutex;

	if ( !opts.upload ) {

		if ( opts.output_type == OutputType::verbose )
			std::cout << "\nTesting download speed (" << profile.download.concurrency << "):" << std::flush;

		speedtest::Speed downloadSpeed;

		if ( sp.download_speed(server, profile.download, downloadSpeed,
		        [&opts, &profile, &output_mutex](bool success, speedtest::Speed current) {
			if ( opts.output_type == OutputType::verbose && success ) {
				std::lock_guard lk(output_mutex);
				std::cout << "\rTesting download speed (" << profile.download.concurrency << "): "
				          << current << "          " << std::flush;
			}
		})) {
			if ( opts.output_type == OutputType::verbose )
				std::cout << "\rDownload: " << downloadSpeed
				          << "                                    \n";
			else if ( opts.output_type == OutputType::text )
				std::cout << "DOWNLOAD_SPEED=" << std::fixed << std::setprecision(2)
				          << downloadSpeed.mbps() << "\n";
			else if ( opts.output_type == OutputType::json )
				std::cout << "\"download\":" << std::fixed << std::setprecision(0)
				          << downloadSpeed.bps()
				          << ",\"download_mbit\":" << std::fixed << std::setprecision(2)
				          << downloadSpeed.mbps() << ",";
		} else {
			if ( opts.output_type == OutputType::json )
				std::cout << "\"error\":\"download test failed\"}\n";
			else if ( opts.output_type == OutputType::verbose )
				std::cout << "\rDownload test failed.\n";
			else
				std::cerr << "\nDownload test failed.\n";
			return EXIT_FAILURE;
		}
	}

	if ( opts.download ) {
		if ( opts.output_type == OutputType::json )
			std::cout << "\"_\":\"only download requested\"}\n";
		return EXIT_SUCCESS;
	}

	if ( opts.output_type == OutputType::verbose )
		std::cout << "Testing upload speed (" << profile.upload.concurrency << "):" << std::flush;

	speedtest::Speed uploadSpeed;

	if ( sp.upload_speed(server, profile.upload, uploadSpeed,
	        [&opts, &profile, &output_mutex](bool success, speedtest::Speed current) {
		if ( opts.output_type == OutputType::verbose && success ) {
			std::lock_guard lk(output_mutex);
			std::cout << "\rTesting upload speed (" << profile.upload.concurrency << "): "
			          << current << "          " << std::flush;
		}
	})) {
		if ( opts.output_type == OutputType::verbose )
			std::cout << "\rUpload: " << uploadSpeed
			          << "                                    \n";
		else if ( opts.output_type == OutputType::text )
			std::cout << "UPLOAD_SPEED=" << std::fixed << std::setprecision(2)
			          << uploadSpeed.mbps() << "\n";
		else if ( opts.output_type == OutputType::json )
			std::cout << "\"upload\":" << std::fixed << std::setprecision(0)
			          << uploadSpeed.bps()
			          << ",\"upload_mbit\":" << std::fixed << std::setprecision(2)
			          << uploadSpeed.mbps() << ",";
	} else {
		if ( opts.output_type == OutputType::json )
			std::cout << "\"error\":\"upload test failed\"}\n";
		else if ( opts.output_type == OutputType::verbose )
			std::cout << "\rUpload test failed.\n";
		else
			std::cerr << "\nUpload test failed.\n";
		return EXIT_FAILURE;
	}

	if ( opts.share ) {
		std::string share_url;
		if ( sp.share(server, share_url)) {
			if ( opts.output_type == OutputType::verbose )
				std::cout << "Results image: " << share_url << "\n";
			else if ( opts.output_type == OutputType::text )
				std::cout << "IMAGE_URL=" << share_url << "\n";
			else if ( opts.output_type == OutputType::json )
				std::cout << "\"share\":\"" << share_url << "\",";
		} else {
			if ( opts.output_type == OutputType::json )
				std::cout << "\"share\":\"\",\"error\":\"shareable result image generation failed\",";
			else
				std::cerr << "Failed to generate shareable result image for results\n";
		}
	}

	if ( opts.output_type == OutputType::json )
		std::cout << "\"_\":\"all ok\"}\n";

	return EXIT_SUCCESS;
}
