#pragma once

#include <string>

namespace speedtest {

	struct IPInfo {

		std::string ip_address;
		std::string isp;
		std::string country;
		float lat, lon;
	};

	struct Server {

		std::string url;
		std::string name;
		std::string country;
		std::string country_code;
		std::string host;
		std::string sponsor;

		int id;
		float lat, lon, distance;

		bool recommended;
	};

}
