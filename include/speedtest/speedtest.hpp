#pragma once

#include <cstdlib>
#include <functional>
#include <cmath>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <curl/curl.h>

#include "speedtest/constants.hpp"
#include "speedtest/profile.hpp"
#include "speedtest/client.hpp"
#include "speedtest/types.hpp"

namespace speedtest {

	// Component-wise "major.minor" comparison. The raw-TCP protocol version is NOT
	// a decimal number: a server reporting "2.11" is NEWER than "2.3" (minor 11 > 3),
	// while a naive float compare (2.11f < 2.3f) falsely rejects every modern server
	// with minor >= 10.
	static inline bool version_ge(const std::string& a, const std::string& b) {
		auto split = [](const std::string& v, int& maj, int& min) {
			size_t dot = v.find('.');
			maj = std::atoi(v.substr(0, dot).c_str());
			min = dot == std::string::npos ? 0 : std::atoi(v.substr(dot + 1).c_str());
		};
		int amaj, amin, bmaj, bmin;
		split(a, amaj, amin);
		split(b, bmaj, bmin);
		return amaj > bmaj || (amaj == bmaj && amin >= bmin);
	}


	using SpeedCallback  = std::function<void(bool success, speedtest::Speed current)>;
	using ServerCallback = std::function<void(bool success, const speedtest::Server&, long latency_ms)>;

	class Client;
	typedef bool (speedtest::Client::*opFn)(long size, long chunk_size, std::atomic<long long>& counter,
	        std::chrono::steady_clock::time_point deadline);

	class SpeedTest {

		public:

			explicit SpeedTest(bool insecure = false, float minServerVersion = speedtest::MIN_SERVER_VERSION);
			~SpeedTest();

			bool ipinfo(speedtest::IPInfo& info);
			bool profile(speedtest::Profile& profile);
			bool set_server(speedtest::Server& server);
			bool set_server(speedtest::Server& server, std::vector<speedtest::Server>& servers);
			void set_insecure(bool insecure = false);
			void set_source_ip(const std::string& ip);

			bool download_speed(const speedtest::Server& server, const speedtest::Config& config, speedtest::Speed& result, speedtest::SpeedCallback cb = nullptr);
			bool upload_speed(const speedtest::Server& server, const speedtest::Config& config, speedtest::Speed& result, speedtest::SpeedCallback cb = nullptr);
			bool jitter(const speedtest::Server& server, double& result, int sample = 40);
			bool share(const speedtest::Server& server, std::string& image_url);

			void reset_servers();
			bool select_recommended_server(speedtest::Server& server);
			const speedtest::Server best_server(int sample_size = 5, speedtest::ServerCallback cb = nullptr);
			const std::vector<speedtest::Server>& servers();
			const long& latency();
			const unsigned long& received();
			const unsigned long& sent();

			static std::map<std::string, std::string> parse_query_string(const std::string& query);
			static std::vector<std::string> split_string(const std::string& instr, char separator);
			static const std::string user_agent();

		private:

			long          _latency;
			speedtest::Speed _uploadSpeed;
			speedtest::Speed _downloadSpeed;
			unsigned long _bytesReceived;
			unsigned long _bytesSent;
			float         _minSupportedServer;
			std::string   _minSupportedVersion;
			std::string   _source_ip;
			bool          _strict_ssl_verify;
			int           _preferred_server_id;
			std::vector<int> _ignored_servers;

			speedtest::IPInfo  _ipinfo;
			speedtest::Server  _server;
			std::vector<speedtest::Server> _servers;
			speedtest::Profile _profile;

			std::string hash_data() const;
			speedtest::Client make_client(const speedtest::Server& server);
			int recommended_server_id(const speedtest::Server& fallback);
			bool get_servers(const std::string& url, std::vector<speedtest::Server>& target, int& http_code);
			bool test_latency(speedtest::Client& client, int sample_size, long& latency);
			speedtest::Speed execute(const speedtest::Server& server, const speedtest::Config& config,
			        unsigned long& bytes_total, const opFn& pfunc, speedtest::SpeedCallback cb = nullptr);
			speedtest::Server find_best_server_in(const std::vector<speedtest::Server>& servers, long& latency,
			        int sample_size = 5, speedtest::ServerCallback cb = nullptr);
			bool parse_servers(const std::string& json, speedtest::IPInfo& info,
			        std::vector<std::map<std::string, std::string>>& servers);

			bool get_config(std::string& data);
			bool get_ip_info(const std::string& data);
			bool get_server_info(const std::string& data);
			bool get_profile_info(const std::string& data);

			static CURL* curl_setup(CURL* curl = nullptr);
			CURLcode http_get(const std::string& url, std::stringstream& os, CURL* handler = nullptr, long timeout = 30);
			CURLcode http_post(const std::string& url, const std::string& postdata, std::stringstream& os, CURL* handler = nullptr, long timeout = 30);

			static size_t write_func(void* buf, size_t size, size_t nmemb, void* userp);
	};

}
