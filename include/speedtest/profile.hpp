#pragma once

#include <string>

namespace speedtest {

	struct Config {

		long start_size, max_size, incr_size, buff_size;
		long min_test_time_ms;
		int concurrency;

		static const speedtest::Config preflight;
        };

	struct Profile {

		speedtest::Config download;
		speedtest::Config upload;
		std::string name;
		std::string description;

		Profile(const double preSpeed);
		Profile(speedtest::Config download, speedtest::Config upload, std::string name, std::string description) :
			download(download), upload(upload), name(name), description(description) {};

		static const speedtest::Profile slowband;
		static const speedtest::Profile narrowband;
		static const speedtest::Profile broadband;
		static const speedtest::Profile fiber;
	};

}
