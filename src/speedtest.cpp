#include <iomanip>
#include <netdb.h>
#include <sys/utsname.h>

#include "speedtest/md5.hpp"
#include "speedtest/speedtest.hpp"

speedtest::SpeedTest::SpeedTest(float minServerVersion): _latency(0), _uploadSpeed(0), _downloadSpeed(0) {
	curl_global_init(CURL_GLOBAL_DEFAULT);
	this -> _ipinfo = speedtest::IPInfo();
	this -> _servers = std::vector<speedtest::Server>();
	this -> _minSupportedServer = minServerVersion;
}

speedtest::SpeedTest::~SpeedTest() {

	curl_global_cleanup();
	this -> _servers.clear();
}

bool speedtest::SpeedTest::ipinfo(speedtest::IPInfo& info) {

	if ( !this -> _ipinfo.ip_address.empty()) {

		info = this -> _ipinfo;
		return true;
	}

	std::stringstream oss;
	if ( speedtest::SpeedTest::http_get(speedtest::IP_INFO_API_URL, oss) == CURLE_OK ) {

		auto values = speedtest::SpeedTest::parse_query_string(oss.str());
		this -> _ipinfo = IPInfo();
		this -> _ipinfo.ip_address = values["ip_address"];
		this -> _ipinfo.isp = values["isp"];
		this -> _ipinfo.lat = std::stof(values["lat"]);
		this -> _ipinfo.lon = std::stof(values["lon"]);
		values.clear();
		oss.clear();
		info = this -> _ipinfo;
		return true;
	}

	return false;
}

const std::vector<speedtest::Server>& speedtest::SpeedTest::servers() {

	if ( !this -> _servers.empty())
		return this -> _servers;

	int http_code = 0;
	if ( this -> get_servers(speedtest::SERVER_LIST_URL, this -> _servers, http_code) && http_code == 200 )
		return this -> _servers;
	else this -> _servers.clear();

	return this -> _servers;
}

void speedtest::SpeedTest::reset_servers() {

	this -> _servers.clear();
}

bool speedtest::SpeedTest::select_recommended_server(speedtest::Server &server) {

	for ( auto &e : this -> servers())

		if ( e.recommended ) {

			speedtest::Client client(e);

			if ( client.connect() && client.version() >= this -> _minSupportedServer &&
				this -> test_latency(client, speedtest::LATENCY_SAMPLE_SIZE, this -> _latency )) {

					server = e;
					client.close();
					return true;
			}

			client.close();
	}

	return false;
}

const speedtest::Server speedtest::SpeedTest::best_server(const int sample_size, std::function<void(bool, const speedtest::Server&, long)> cb) {

	auto best = this -> find_best_server_in(this -> servers(), this -> _latency, sample_size, cb);
	speedtest::Client client(best);
	this -> test_latency(client, speedtest::LATENCY_SAMPLE_SIZE, this -> _latency);
	client.close();
	return best;
}

bool speedtest::SpeedTest::set_server(speedtest::Server& server) {

	speedtest::Client client(server);

	if ( client.connect() && client.version() >= this -> _minSupportedServer &&
		this -> test_latency(client, speedtest::LATENCY_SAMPLE_SIZE, this -> _latency )) {

			client.close();
			return true;
	}

	client.close();
	return false;
}

bool speedtest::SpeedTest::download_speed(const speedtest::Server &server, const speedtest::Config& config, double& result, std::function<void(bool, double)> cb) {

	opFn pfunc = &speedtest::Client::download;
	this -> _downloadSpeed = this -> execute(server, config, pfunc, cb);
	result = this -> _downloadSpeed;
	return true;
}

bool speedtest::SpeedTest::upload_speed(const speedtest::Server &server, const speedtest::Config& config, double& result, std::function<void(bool, double)> cb) {

	opFn pfunc = &speedtest::Client::upload;
	this -> _uploadSpeed = this -> execute(server, config, pfunc, cb);
	result = this -> _uploadSpeed;
	return true;
}

const long &speedtest::SpeedTest::latency() {

	return this -> _latency;
}

bool speedtest::SpeedTest::jitter(const speedtest::Server &server, long& result, const int sample) {

	auto client = speedtest::Client(server);
	double current_jitter = 0;
	long previous_ms = LONG_MAX;

	if ( !client.connect())
		return false;

	for ( int i = 0; i < sample; i++ ) {

		long ms = 0;

		if ( client.ping(ms)) {
			if ( previous_ms == LONG_MAX )
				previous_ms = ms;
			else current_jitter += std::abs(previous_ms - ms);
		}
	}

	client.close();
	result = (long) std::floor(current_jitter / sample);
	return true;
}

bool speedtest::SpeedTest::share(const speedtest::Server& server, std::string& image_url) {

	std::stringstream hash_data, post_data, result;
	long http_code = 0;

	image_url.clear();

	hash_data << std::setprecision(0) << std::fixed << this -> _latency <<
		"-" << std::setprecision(2) << std::fixed << ( this -> _uploadSpeed * 1000 ) <<
		"-" << std::setprecision(2) << std::fixed << ( this -> _downloadSpeed * 1000 ) <<
		"-" << speedtest::API_KEY;

	std::string hash = speedtest::md5(hash_data.str());

	/*
	post_data << "download=" << std::setprecision(2) << std::fixed << ( this -> _downloadSpeed * 1000 ) << "&" <<
		"ping=" << std::setprecision(0) << std::fixed << this -> _latency << "&" <<
		"upload=" << std::setprecision(2) << std::fixed << ( this -> _uploadSpeed * 1000 ) << "&" <<
		"pingselect=1&" <<
		"recommendedserverid=" << server.id << "&" <<
		"accuracy=1&" <<
		"serverid=" << server.id << "&" <<
		"hash=" << hash;
	*/

	speedtest::Server recommended_server;
	bool recommended = this -> select_recommended_server(recommended_server);

	post_data <<
		"recommendedserverid=" << ( recommended ? recommended_server.id : server.id ) << "&" <<
		"ping=" << std::setprecision(0) << std::fixed << this -> _latency << "&" <<
		"screenresolution=&" <<
		"screendpi=&" <<
		"promo=&" <<
		"download=" << std::setprecision(2) << std::fixed << ( this -> _downloadSpeed * 1000 ) << "&" <<
		"upload=" << std::setprecision(2) << std::fixed << ( this -> _uploadSpeed * 1000 ) << "&" <<
		"testmethod=http&" <<
		"hash=" << hash << "&" <<
		"touchscreen=none&" <<
		"startmode=pingselect&" <<
		"accuracy=1&" <<
		"serverid=" << server.id;

	CURL *c = curl_easy_init();
	curl_easy_setopt(c, CURLOPT_REFERER, speedtest::API_REFERER_URL.c_str());

	if ( speedtest::SpeedTest::http_post(speedtest::API_URL, post_data.str(), result, c) == CURLE_OK ) {

		curl_easy_getinfo(c, CURLINFO_HTTP_CODE, &http_code);

		if ( http_code == 200 && !result.str().empty()) {

			auto data = speedtest::SpeedTest::parse_query_string(result.str());
			if ( data.count("resultid") == 1 )
				image_url = "http://www.speedtest.net/result/" + data["resultid"] + ".png";
		}
	}

	curl_easy_cleanup(c);
	return !image_url.empty();
}

// private

const std::string speedtest::SpeedTest::user_agent() {

	struct utsname buf;

	if ( uname(&buf))
		return "Mozilla/5.0 Linux-1; U; x86_64; en-us (KHTML, like Gecko) SpeedTestCpp/" + speedtest::version;

	std::stringstream ss;

	ss << "Mozilla/5.0 " << buf.sysname << "-" << buf.release << "; U; " << buf.machine << "; en-us (KHTML, like Gecko) SpeedTestCpp/" + speedtest::version;
	return ss.str();
}

double speedtest::SpeedTest::execute(const speedtest::Server &server, const speedtest::Config &config, const opFn &pfunc, std::function<void(bool, double)> cb) {

	std::vector<std::thread> workers;
	double overall_speed = 0;
	std::mutex mtx;

	for ( int i = 0; i < config.concurrency; i++ ) {

		workers.push_back(std::thread([&server, &overall_speed, &pfunc, &config, &mtx, cb]() {

			speedtest::Client client(server);

			long start_size = config.start_size;
			long max_size   = config.max_size;
			long incr_size  = config.incr_size;
			long curr_size  = start_size;

			if ( client.connect()) {

				long total_size = 0;
				long total_time = 0;
				auto start = std::chrono::steady_clock::now();
				std::vector<double> partial_results;

				while ( curr_size < max_size ) {

					long op_time = 0;

					if (( client.*pfunc)(curr_size, config.buff_size, op_time)) {

						total_size += curr_size;
						total_time += op_time;
						double metric = ( curr_size * 8 ) / (static_cast<double>(op_time) / 1000);
						partial_results.push_back(metric);

						if ( cb ) cb(true, metric);
					} else if ( cb ) cb(false, -1);

					curr_size += incr_size;
					auto stop = std::chrono::steady_clock::now();

					if ( std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() > config.min_test_time_ms )
						break;
				}

				client.close();
				std::sort(partial_results.begin(), partial_results.end());

				size_t skip = partial_results.size() >= 10 ? ( partial_results.size() / 4 ) : 0;
				size_t drop = partial_results.size() >= 10 ? 2 : 0;
				size_t iter = 0;
				double real_sum = 0;

				for ( auto it = partial_results.begin() + skip; it != partial_results.end() - drop; ++it ) {

					iter++;
					real_sum += (*it);
				}

				mtx.lock();
				overall_speed += ( real_sum / iter );
				mtx.unlock();
			} else if ( cb ) cb(false, -1);
		}));

	}

	for ( auto &t : workers )
		t.join();

	workers.clear();

	return overall_speed / 1000 / 1000;
}

CURLcode speedtest::SpeedTest::http_get(const std::string &url, std::stringstream &ss, CURL *handler, long timeout) {

	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = speedtest::SpeedTest::curl_setup(handler);

	if ( curl ) {

		if ( CURLE_OK == ( code = curl_easy_setopt(curl, CURLOPT_FILE, &ss)) &&
			CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout)) &&
			CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this -> _strict_ssl_verify)) &&
			CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
			code = curl_easy_perform(curl);

		if ( handler == nullptr )
			curl_easy_cleanup(curl);
	}

	return code;
}

CURLcode speedtest::SpeedTest::http_post(const std::string &url, const std::string &postdata, std::stringstream &os, void *handler, long timeout) {

	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = speedtest::SpeedTest::curl_setup(handler);

	if ( curl ) {

		if ( CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os)) &&
			CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout)) &&
			CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())) &&
			CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this -> _strict_ssl_verify)) &&
			CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata.c_str())))
			code = curl_easy_perform(curl);

		if ( handler == nullptr )
			curl_easy_cleanup(curl);
	}

	return code;
}

CURL *speedtest::SpeedTest::curl_setup(CURL *handler) {

	CURL* curl = handler == nullptr ? curl_easy_init() : handler;

	if ( curl ) {

		if ( curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &speedtest::SpeedTest::write_func) == CURLE_OK &&
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L) == CURLE_OK &&
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&
			curl_easy_setopt(curl, CURLOPT_USERAGENT, speedtest::SpeedTest::user_agent().c_str()) == CURLE_OK ) return curl;
		else {
			curl_easy_cleanup(handler);
			return nullptr;
		}
	}

	return nullptr;
}

size_t speedtest::SpeedTest::write_func(void *buf, size_t size, size_t nmemb, void *userp) {

	if ( !userp )
		return 0;

	std::stringstream &os = *static_cast<std::stringstream *>(userp);
	std::streamsize len = size * nmemb;

	if ( os.write(static_cast<char*>(buf), len))
		return static_cast<size_t>(len);
	else return 0;
}

std::map<std::string, std::string> speedtest::SpeedTest::parse_query_string(const std::string &query) {

	auto map = std::map<std::string, std::string>();
	auto pairs = speedtest::SpeedTest::split_string(query, '&');

	for ( auto &p : pairs ) {
		auto kv = speedtest::SpeedTest::split_string(p, '=');
		if ( kv.size() == 2 )
			map[kv[0]] = kv[1];
	}

	return map;
}

std::vector<std::string> speedtest::SpeedTest::split_string(const std::string &instr, const char separator) {

	if ( instr.empty())
		return std::vector<std::string>();

	std::vector<std::string> tokens;
	std::size_t start = 0, end = 0;

	while (( end = instr.find(separator, start)) != std::string::npos ) {

		if ( std::string temp = instr.substr(start, end - start); !temp.empty())
			tokens.push_back(temp);
		start = end + 1;
	}

	if ( std::string temp = instr.substr(start); !temp.empty())
		tokens.push_back(temp);

	return tokens;
}

bool speedtest::SpeedTest::get_servers(const std::string& url, std::vector<speedtest::Server>& target, int &http_code) {

	std::stringstream oss;
	target.clear();

	auto isHttpScheme = url.find_first_of("http") == 0;

	CURL* curl = curl_easy_init();
	if ( speedtest::SpeedTest::http_get(url, oss, curl, 20) != CURLE_OK )
		return false;

	if ( isHttpScheme ) {

		int req_status;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &req_status);
		http_code = req_status;

		if ( http_code != 200 ) {

			curl_easy_cleanup(curl);
			return false;
		}
	} else http_code = 200;

	speedtest::IPInfo ipInfo;

	if ( !this -> ipinfo(ipInfo)) {

		curl_easy_cleanup(curl);
		std::cerr << "OOPS!" << std::endl;
		return false;
	}

	std::vector<std::map<std::string, std::string>> values;

	if ( !this -> parse_servers(oss.str(), ipInfo, values))
		return false;

	for ( auto &server : values ) {

		speedtest::Server info = {
			server["url"], server["name"], server["country"], server["cc"],
			server["host"], server["sponsor"], std::stoi(server["id"]),
			std::stof(server["lat"]), std::stof(server["lon"]),
			std::stof(server["distance"]),
			server.contains("force_ping_select") ? (
				server["force_ping_select"] == "1" ? true : false ) : false
		};

		if ( !info.url.empty())
			target.push_back(info);
	}

	curl_easy_cleanup(curl);
	std::sort(target.begin(), target.end(), [](const speedtest::Server &a, const speedtest::Server &b) -> bool {
		return a.distance < b.distance;
	});

	return true;
}

const speedtest::Server speedtest::SpeedTest::find_best_server_in(const std::vector<speedtest::Server> &servers, long &latency,
                                                 const int sample_size, std::function<void(bool, const speedtest::Server&, long)> cb) {

	int i = sample_size;
	speedtest::Server best = servers[0];

	latency = INT_MAX;

	for ( auto &server : servers ) {

		speedtest::Client client(server);

		if ( !client.connect()) {

			if ( cb ) cb(false, server, (long)-1);
			continue;
		}

		if ( client.version() < _minSupportedServer ) {

			client.close();
			continue;
		}

		long current_latency = LONG_MAX;

		if ( this -> test_latency(client, 20, current_latency) && current_latency < latency ) {

			latency = current_latency;
			best = server;
		}

		client.close();

		if ( cb ) cb(true, server, current_latency);
		if ( i-- < 0 )
			break;
	}

	return best;
}

bool speedtest::SpeedTest::test_latency(speedtest::Client &client, const int sample_size, long &latency) {

	if ( !client.connect())
		return false;

	latency = INT_MAX;
	long temp_latency = 0;

	for ( int i = 0; i < sample_size; i++ ) {

		if ( client.ping(temp_latency)) {
			if ( temp_latency < latency )
				latency = temp_latency;
		} else return false;
	}

	return true;
}

void speedtest::SpeedTest::set_insecure(bool insecure) {
    // when insecure is on, we dont want ssl cert to be verified.
    // when insecure is off, we want ssl cert to be verified.
    this -> _strict_ssl_verify = !insecure;
}
