#pragma once
#include <string>
#include <utility>

namespace speedtest {

	static const std::string version = "1.20.0";

#ifdef SPEEDTEST_GIT_COMMIT
	static const std::string git_commit = SPEEDTEST_GIT_COMMIT;
#else
	static const std::string git_commit = "";
#endif

#ifdef OLD_SERVER_LIST
	static const std::string SERVER_LIST_URL = "https://www.speedtest.net/speedtest-servers.php";
#else
	static const std::string SERVER_LIST_URL = "https://www.speedtest.net/api/js/servers?engine=js&limit=10&https_functional=true";
#endif

	static const std::string IP_INFO_API_URL = "http://speedtest.ookla.com/api/ipaddress.php";
	static const std::string API_URL = "http://www.speedtest.net/api/api.php";
	static const std::string API_REFERER_URL = "http://c.speedtest.net/flash/speedtest.swf";
	static const std::string API_KEY = "297aae72";
	static const float MIN_SERVER_VERSION = 2.3;
	static const size_t LATENCY_SAMPLE_SIZE = 80;
}
