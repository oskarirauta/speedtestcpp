#pragma once

#include <string>

namespace speedtest {

	const bool hex_digest(const std::string &str, std::string &result);
	const std::string user_agent();
}
