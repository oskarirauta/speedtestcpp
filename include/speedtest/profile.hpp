#pragma once

#include <string>
#include "speedtest/types.hpp"

namespace speedtest {

	struct Config {

		long start_size       = 0;
		long max_size         = 0;
		long incr_size        = 0;
		long buff_size        = 0;
		long min_test_time_ms = 0;
		int  concurrency      = 1;

		static const speedtest::Config preflight;
	};

	struct Profile {

		speedtest::Config download;
		speedtest::Config upload;
		std::string name;
		std::string description;

		explicit Profile(speedtest::Speed preSpeed);
		Profile(speedtest::Config dl, speedtest::Config ul, std::string name, std::string desc)
			: download(dl), upload(ul), name(std::move(name)), description(std::move(desc)) {}

		static speedtest::Profile uninitialized();
		static speedtest::Profile slowband(speedtest::Speed preSpeed = {});
		static speedtest::Profile narrowband();
		static speedtest::Profile broadband();
		static speedtest::Profile fiber();
	};

}
