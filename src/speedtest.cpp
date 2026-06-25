#include <iomanip>
#include <atomic>
#include <climits>
#include <deque>
#include <netdb.h>
#include <sys/utsname.h>

#include "speedtest/md5.hpp"
#include "speedtest/xml.hpp"
#include "speedtest/speedtest.hpp"

speedtest::SpeedTest::SpeedTest(bool insecure, float minServerVersion)
    : _latency(0), _uploadSpeed({}), _downloadSpeed({}),
      _bytesReceived(0), _bytesSent(0),
      _profile(speedtest::Profile::uninitialized()) {

	curl_global_init(CURL_GLOBAL_DEFAULT);
	_strict_ssl_verify  = !insecure;
	_preferred_server_id = -1;
	_minSupportedServer = minServerVersion;

	std::string data;
	if ( get_config(data)) {
		if ( !get_ip_info(data) )  return;
		if ( !get_server_info(data) ) return;
		get_profile_info(data);
	}
}

speedtest::SpeedTest::~SpeedTest() {
	curl_global_cleanup();
}

bool speedtest::SpeedTest::ipinfo(speedtest::IPInfo& info) {

	if ( !_ipinfo.ip_address.empty()) {
		info = _ipinfo;
		return true;
	}

	std::string data;
	if ( !get_config(data) || !get_ip_info(data))
		return false;

	info = _ipinfo;
	return true;
}

bool speedtest::SpeedTest::profile(speedtest::Profile& profile) {

	if ( _profile.download.concurrency < 1 ) {
		std::string data;
		if ( !get_config(data) || !get_profile_info(data))
			return false;
	}

	profile = _profile;
	return true;
}

const std::vector<speedtest::Server>& speedtest::SpeedTest::servers() {

	if ( !_servers.empty())
		return _servers;

	int http_code = 0;
	if ( !get_servers(speedtest::SERVER_LIST_URL, _servers, http_code) || http_code != 200 )
		_servers.clear();

	return _servers;
}

void speedtest::SpeedTest::reset_servers() {
	_servers.clear();
}

bool speedtest::SpeedTest::select_recommended_server(speedtest::Server& server) {

	if ( _servers.empty()) {
		int http_code = 0;
		if ( !get_servers(speedtest::SERVER_LIST_URL, _servers, http_code) || http_code != 200 )
			return false;
	}

	if ( _servers.empty())
		return false;

	for ( auto& e : servers()) {

		if ( !e.recommended )
			continue;

		speedtest::Client client(e);

		if ( client.connect() && client.version() >= _minSupportedServer &&
		     test_latency(client, speedtest::LATENCY_SAMPLE_SIZE, _latency)) {
			server = e;
			client.close();
			return true;
		}

		client.close();
	}

	return false;
}

const speedtest::Server speedtest::SpeedTest::best_server(int sample_size, speedtest::ServerCallback cb) {

	auto best = find_best_server_in(servers(), _latency, sample_size, cb);
	speedtest::Client client(best);
	test_latency(client, speedtest::LATENCY_SAMPLE_SIZE, _latency);
	client.close();
	return best;
}

bool speedtest::SpeedTest::set_server(speedtest::Server& server) {

	speedtest::Client client(server);

	if ( client.connect() && client.version() >= _minSupportedServer &&
	     test_latency(client, speedtest::LATENCY_SAMPLE_SIZE, _latency)) {
		client.close();
		return true;
	}

	client.close();
	return false;
}

bool speedtest::SpeedTest::set_server(speedtest::Server& server, std::vector<speedtest::Server>& servers) {

	if ( !set_server(server))
		return false;

	for ( auto& s : servers )
		if ( s.host == server.host )
			server = s;

	return true;
}

bool speedtest::SpeedTest::download_speed(const speedtest::Server& server, const speedtest::Config& config,
        speedtest::Speed& result, speedtest::SpeedCallback cb) {

	opFn pfunc = &speedtest::Client::download;
	_downloadSpeed = execute(server, config, _bytesReceived, pfunc, cb);
	result = _downloadSpeed;
	return true;
}

bool speedtest::SpeedTest::upload_speed(const speedtest::Server& server, const speedtest::Config& config,
        speedtest::Speed& result, speedtest::SpeedCallback cb) {

	opFn pfunc = &speedtest::Client::upload;
	_uploadSpeed = execute(server, config, _bytesSent, pfunc, cb);
	result = _uploadSpeed;
	return true;
}

const long& speedtest::SpeedTest::latency() {
	return _latency;
}

const unsigned long& speedtest::SpeedTest::received() {
	return _bytesReceived;
}

const unsigned long& speedtest::SpeedTest::sent() {
	return _bytesSent;
}

bool speedtest::SpeedTest::jitter(const speedtest::Server& server, long& result, const int sample) {

	speedtest::Client client(server);

	if ( !client.connect())
		return false;

	double current_jitter = 0;
	double previous_ms   = -1;
	int    measurements  = 0;

	for ( int i = 0; i < sample; i++ ) {

		double ms = 0;

		if ( client.ping(ms)) {
			if ( previous_ms >= 0 ) {
				current_jitter += std::abs(previous_ms - ms);
				measurements++;
			}
			previous_ms = ms;
		}
	}

	client.close();
	result = measurements > 0 ? static_cast<long>(std::floor(current_jitter / measurements)) : 0;
	return true;
}

bool speedtest::SpeedTest::share(const speedtest::Server& server, std::string& image_url) {

	std::stringstream post_data, result;
	long http_code = 0;

	image_url.clear();

	post_data <<
		"recommendedserverid=" << recommended_server_id(server) << "&" <<
		"ping="      << std::setprecision(0) << std::fixed << _latency << "&" <<
		"screendpi=&" <<
		"promo=&" <<
		"download="  << std::setprecision(2) << std::fixed << _downloadSpeed.kbps() << "&" <<
		"upload="    << std::setprecision(2) << std::fixed << _uploadSpeed.kbps()   << "&" <<
		"testmethod=http&" <<
		"hash="      << speedtest::md5(hash_data()) << "&" <<
		"touchscreen=none&" <<
		"startmode=pingselect&" <<
		"accuracy=1&" <<
		"bytesreceived=" << _bytesReceived << "&" <<
		"bytessent="     << _bytesSent     << "&" <<
		"serverid="      << server.id;

	CURL* c = curl_easy_init();
	curl_easy_setopt(c, CURLOPT_REFERER, speedtest::API_REFERER_URL.c_str());

	if ( http_post(speedtest::API_URL, post_data.str(), result, c) == CURLE_OK ) {

		curl_easy_getinfo(c, CURLINFO_HTTP_CODE, &http_code);

		if ( http_code == 200 && !result.str().empty()) {
			auto data = parse_query_string(result.str());
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
	ss << "Mozilla/5.0 " << buf.sysname << "-" << buf.release
	   << "; U; " << buf.machine
	   << "; en-us (KHTML, like Gecko) SpeedTestCpp/" << speedtest::version;
	return ss.str();
}

std::string speedtest::SpeedTest::hash_data() const {

	std::stringstream s;
	s << std::setprecision(0) << std::fixed << _latency
	  << "-" << std::setprecision(2) << std::fixed << _uploadSpeed.kbps()
	  << "-" << std::setprecision(2) << std::fixed << _downloadSpeed.kbps()
	  << "-" << speedtest::API_KEY;
	return s.str();
}

int speedtest::SpeedTest::recommended_server_id(const speedtest::Server& fallback) {

	speedtest::Server server;
	if ( select_recommended_server(server))
		return server.id;
	return fallback.id;
}

speedtest::Speed speedtest::SpeedTest::execute(const speedtest::Server& server, const speedtest::Config& config,
        unsigned long& bytes_total, const opFn& pfunc, speedtest::SpeedCallback cb) {

	using namespace std::chrono;

	std::atomic<long long> shared_bytes{0};
	std::atomic<int>       active{config.concurrency};
	std::mutex             cb_mtx;

	bytes_total = 0;
	const auto test_start = steady_clock::now();
	const auto deadline   = test_start + milliseconds(config.min_test_time_ms);

	auto elapsed_sec = [&]() -> double {
		return duration_cast<microseconds>(steady_clock::now() - test_start).count() / 1e6;
	};

	// Worker threads: transfer until the deadline. Byte counting happens inside
	// the client per read/write, and a worker may stop mid-block at the deadline.
	std::vector<std::thread> workers;
	for ( int i = 0; i < config.concurrency; i++ ) {

		workers.emplace_back([&, i]() {

			speedtest::Client client(server);

			if ( !client.connect()) {
				active--;
				if ( cb ) { std::lock_guard lk(cb_mtx); cb(false, {}); }
				return;
			}

			// Stagger the per-worker block size so the concurrent connections
			// do not all reach their block boundary (and the upload OK-handshake
			// stall that follows it) at the same instant. The offset spans a full
			// block (start_size), spreading the block boundaries evenly across one
			// block period; a smaller span would leave the connections clustered
			// and the aggregate byte rate would still pulse. The phase offset
			// persists across rounds since every worker grows by the same incr.
			long size = config.concurrency > 1
				? std::min(config.start_size + (config.start_size * i) / config.concurrency,
				           config.max_size)
				: config.start_size;

			while ( steady_clock::now() < deadline ) {

				if ( !(client.*pfunc)(size, config.buff_size, shared_bytes, deadline)) {
					if ( cb ) { std::lock_guard lk(cb_mtx); cb(false, {}); }
					break;
				}

				size = std::min(size + config.incr_size, config.max_size);
			}

			client.close();
			active--;
		});
	}

	// Reporter thread: emit the average speed over a trailing fixed-duration
	// window (total bytes in the window / its real span). A true windowed
	// average rejects the per-window burstiness (TCP send-buffer fill/drain
	// plus the upload OK-handshake gaps) far better than an EMA, which keeps
	// chasing each recent spike. The window bounds the lag to its width.
	constexpr auto kWindow = milliseconds(2000);
	std::thread reporter([&]() {

		// Trailing samples of (timestamp, cumulative bytes) within the window.
		std::deque<std::pair<steady_clock::time_point, long long>> samples;
		bool started = false;

		while ( active.load() > 0 ) {

			std::this_thread::sleep_for(milliseconds(150));

			auto      now   = steady_clock::now();
			long long bytes = shared_bytes.load();

			// Hold off until the transfer has actually started.
			if ( !started ) {
				if ( bytes <= 0 )
					continue;
				started = true;
			}

			samples.emplace_back(now, bytes);

			// Drop samples that fell out of the window, keeping the most recent
			// one that predates it so the average spans the full window width.
			while ( samples.size() > 1 && samples[1].first <= now - kWindow )
				samples.pop_front();

			double    dt = duration_cast<microseconds>(now - samples.front().first).count() / 1e6;
			long long db = bytes - samples.front().second;

			if ( cb && dt > 0.0 && db >= 0 ) {
				std::lock_guard lk(cb_mtx);
				cb(true, speedtest::Speed::from_bytes_per_sec(db / dt));
			}
		}
	});

	for ( auto& t : workers ) t.join();
	double    sec   = elapsed_sec();
	long long bytes = shared_bytes.load();
	reporter.join();

	bytes_total = static_cast<unsigned long>(bytes);

	if ( sec <= 0.0 || bytes == 0 )
		return {};

	return speedtest::Speed::from_bytes_per_sec(bytes / sec);
}

CURLcode speedtest::SpeedTest::http_get(const std::string& url, std::stringstream& ss, CURL* handler, long timeout) {

	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = curl_setup(handler);

	if ( curl ) {
		if ( CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &ss))          &&
		     CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout))   &&
		     CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, _strict_ssl_verify)) &&
		     CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
			code = curl_easy_perform(curl);

		if ( handler == nullptr )
			curl_easy_cleanup(curl);
	}

	return code;
}

CURLcode speedtest::SpeedTest::http_post(const std::string& url, const std::string& postdata,
        std::stringstream& os, CURL* handler, long timeout) {

	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = curl_setup(handler);

	if ( curl ) {
		if ( CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os))          &&
		     CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout))   &&
		     CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str()))   &&
		     CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, _strict_ssl_verify)) &&
		     CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata.c_str())))
			code = curl_easy_perform(curl);

		if ( handler == nullptr )
			curl_easy_cleanup(curl);
	}

	return code;
}

CURL* speedtest::SpeedTest::curl_setup(CURL* handler) {

	CURL* curl = handler == nullptr ? curl_easy_init() : handler;

	if ( curl ) {
		if ( curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &speedtest::SpeedTest::write_func) == CURLE_OK &&
		     curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L)          == CURLE_OK &&
		     curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L)       == CURLE_OK &&
		     curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent().c_str()) == CURLE_OK )
			return curl;

		curl_easy_cleanup(handler);
		return nullptr;
	}

	return nullptr;
}

size_t speedtest::SpeedTest::write_func(void* buf, size_t size, size_t nmemb, void* userp) {

	if ( !userp ) return 0;

	auto& os = *static_cast<std::stringstream*>(userp);
	std::streamsize len = static_cast<std::streamsize>(size * nmemb);
	return os.write(static_cast<char*>(buf), len) ? static_cast<size_t>(len) : 0;
}

std::map<std::string, std::string> speedtest::SpeedTest::parse_query_string(const std::string& query) {

	std::map<std::string, std::string> map;

	for ( auto& p : split_string(query, '&')) {
		auto kv = split_string(p, '=');
		if ( kv.size() == 2 )
			map[kv[0]] = kv[1];
	}

	return map;
}

std::vector<std::string> speedtest::SpeedTest::split_string(const std::string& instr, const char separator) {

	if ( instr.empty())
		return {};

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

bool speedtest::SpeedTest::get_servers(const std::string& url, std::vector<speedtest::Server>& target, int& http_code) {

	std::stringstream oss;
	target.clear();

	bool is_http = url.find("http") == 0;

	CURL* curl = curl_easy_init();
	if ( http_get(url, oss, curl, 20) != CURLE_OK ) {
		curl_easy_cleanup(curl);
		return false;
	}

	if ( is_http ) {
		int req_status = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &req_status);
		http_code = req_status;

		if ( http_code != 200 ) {
			curl_easy_cleanup(curl);
			return false;
		}
	} else http_code = 200;

	speedtest::IPInfo ip_info;
	if ( !ipinfo(ip_info)) {
		curl_easy_cleanup(curl);
		return false;
	}

	std::vector<std::map<std::string, std::string>> values;
	if ( !parse_servers(oss.str(), ip_info, values)) {
		curl_easy_cleanup(curl);
		return false;
	}

	for ( auto& s : values ) {

		if ( s["url"].empty()) continue;

		speedtest::Server info;
		info.url          = s["url"];
		info.name         = s["name"];
		info.country      = s["country"];
		info.country_code = s["cc"];
		info.host         = s["host"];
		info.sponsor      = s["sponsor"];
		info.id           = std::stoi(s["id"]);
		info.lat          = std::stof(s["lat"]);
		info.lon          = std::stof(s["lon"]);
		info.distance     = std::stof(s["distance"]);
		info.recommended  = s.contains("force_ping_select") && s.at("force_ping_select") == "1";

		target.push_back(info);
	}

	curl_easy_cleanup(curl);

	std::sort(target.begin(), target.end(), [](const speedtest::Server& a, const speedtest::Server& b) {
		return a.distance < b.distance;
	});

	return true;
}

speedtest::Server speedtest::SpeedTest::find_best_server_in(const std::vector<speedtest::Server>& servers,
        long& latency, int sample_size, speedtest::ServerCallback cb) {

	speedtest::Server best = servers[0];
	latency = LONG_MAX;
	int remaining = sample_size;

	for ( auto& server : servers ) {

		speedtest::Client client(server);

		if ( !client.connect()) {
			if ( cb ) cb(false, server, -1);
			continue;
		}

		if ( client.version() < _minSupportedServer ) {
			client.close();
			continue;
		}

		long current_latency = LONG_MAX;

		if ( test_latency(client, 20, current_latency) && current_latency < latency ) {
			latency = current_latency;
			best    = server;
		}

		client.close();

		if ( cb ) cb(true, server, current_latency);
		if ( --remaining < 0 ) break;
	}

	return best;
}

bool speedtest::SpeedTest::test_latency(speedtest::Client& client, const int sample_size, long& latency) {

	if ( !client.connect())
		return false;

	latency = LONG_MAX;

	for ( int i = 0; i < sample_size; i++ ) {

		double ms = 0;

		if ( !client.ping(ms)) return false;
		if ( static_cast<long>(ms) < latency )
			latency = static_cast<long>(ms);
	}

	return true;
}

void speedtest::SpeedTest::set_insecure(bool insecure) {
	_strict_ssl_verify = !insecure;
}
