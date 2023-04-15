#pragma once

#include <functional>
#include <cmath>
#include <fstream>
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

	class Client;
	typedef bool (speedtest::Client::*opFn)(const long size, const long chunk_size, long &ms);
	typedef void (*progressFn)(bool success);

	class SpeedTest {

		public:

			explicit SpeedTest(float minServerVersion = speedtest::MIN_SERVER_VERSION);
			~SpeedTest();

			bool ipinfo(speedtest::IPInfo& info);
			bool set_server(speedtest::Server& server);
			void set_insecure(bool insecure = false);

			bool download_speed(const speedtest::Server& server, const speedtest::Config& config, double& result, std::function<void(bool, double)> cb = nullptr);
			bool upload_speed(const speedtest::Server& server, const speedtest::Config& config, double& result, std::function<void(bool, double)> cb = nullptr);
			bool jitter(const speedtest::Server& server, long& result, int sample = 40);
			bool share(const speedtest::Server& server, std::string& image_url);

			void reset_servers();
			bool select_recommended_server(speedtest::Server &server);
			const speedtest::Server best_server(int sample_size = 5, std::function<void(bool, const speedtest::Server&, long)> cb = nullptr);
			const std::vector<speedtest::Server>& servers();
			const long &latency();

			static std::map<std::string, std::string> parse_query_string(const std::string& query);
			static std::vector<std::string> split_string(const std::string& instr, char separator);


		private:

			long _latency;
			double _uploadSpeed;
			double _downloadSpeed;
			float _minSupportedServer;
			bool _strict_ssl_verify;
			speedtest::IPInfo _ipinfo;
			std::vector<speedtest::Server> _servers;

			bool get_servers(const std::string& url,  std::vector<speedtest::Server>& target, int &http_code);
			bool test_latency(speedtest::Client& client, int sample_size, long& latency);
			double execute(const speedtest::Server &server, const speedtest::Config &config, const opFn &fnc, std::function<void(bool, double)> cb = nullptr);
			const speedtest::Server find_best_server_in(const std::vector<speedtest::Server>& servers, long& latency, int sample_size = 5, std::function<void(bool, const speedtest::Server&, long)> cb = nullptr);
			bool parse_servers(const std::string json, speedtest::IPInfo& info, std::vector<std::map<std::string, std::string>> &servers);
			const bool get_hash(std::string &hash);

			static CURL* curl_setup(CURL* curl = nullptr);
			CURLcode http_get(const std::string& url, std::stringstream& os, CURL *handler = nullptr, long timeout = 30);
			CURLcode http_post(const std::string& url, const std::string& postdata, std::stringstream& os, CURL *handler = nullptr, long timeout = 30);

			static size_t write_func(void* buf, size_t size, size_t nmemb, void* userp);
	};

}
