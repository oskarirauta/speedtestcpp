#include <list>

#include "speedtest/xml.hpp"
#include "speedtest/speedtest.hpp"

bool is_number(const std::string& s) {
	return !s.empty() && std::find_if(s.begin(),
		s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

bool speedtest::SpeedTest::get_config(std::string &data) {

	std::stringstream oss;

	if ( speedtest::SpeedTest::http_get(speedtest::CONFIG_URL, oss) == CURLE_OK ) {
		data = oss.str();
		return true;
	}

	return false;
}

bool speedtest::SpeedTest::get_ip_info(const std::string &data) {

	speedtest::xml config(data);
	std::map<std::string, std::string> config_info;

	if ( config.parse("client", { "ip", "lat", "lon", "isp", "country" }, config_info) &&
		speedtest::xml::all_attributes_found({"ip", "isp"}, config_info)) {

		this -> _ipinfo = speedtest::IPInfo(
			config_info["ip"],
			config_info["isp"],
			config_info.contains("country") ? config_info["country"] : "",
			config_info.contains("lat") ? std::stof(config_info["lat"]) : 0,
			config_info.contains("lon") ? std::stof(config_info["lon"]) : 0
		);

		return true;

	}

	this -> _ipinfo.ip_address = "";
	return false;
}

bool speedtest::SpeedTest::get_server_info(const std::string &data) {

	speedtest::xml config(data);
	std::map<std::string, std::string> config_info;

	if ( config.parse("server-config", { "ignoreids" }, config_info) &&
		config_info.contains("ignoreids")) {

		std::vector<std::string> ignored_ids =
			speedtest::SpeedTest::split_string(config_info["ignoreids"], ',');

		for ( const std::string &val : ignored_ids )
			if ( int id = std::stoi(val); id > 0 )
				this -> _ignored_servers.push_back(id);
	}

	return true;
}

bool speedtest::SpeedTest::get_profile_info(const std::string &data) {

	speedtest::xml config(data);

	int ul_ratio, ul_cnt;
	long bufsize = 10240;
	long ul_maxchunkcount, ul_start, ul_max, ul_incr, ul_length, ul_concurrency;

	long dl_length, dl_concurrency;

	std::map<std::string, std::string> upload_info;
	std::map<std::string, std::string> server_info;
	std::map<std::string, std::string> download_info;

	if ( config.parse("upload", { "testlength", "ratio", "mintestsize", "threads", "maxchunksize", "maxchunkcount", "threadsperurl" }, upload_info) &&
		speedtest::xml::all_attributes_found({"testlength", "ratio", "mintestsize", "threads", "maxchunksize", "maxchunkcount", "threadsperurl"}, upload_info) &&
			is_number(upload_info["ratio"]) && is_number(upload_info["maxchunkcount"]) &&
			is_number(upload_info["testlength"]) && is_number(upload_info["threads"])) {

		ul_ratio = std::stoi(upload_info["ratio"]);
		ul_maxchunkcount = std::stoi(upload_info["maxchunkcount"]);
		ul_length = std::stoi(upload_info["testlength"]);
		ul_concurrency = std::stoi(upload_info["threads"]);

		switch ( ul_ratio > 6 ? 6 : ul_ratio ) {
			case 2:
				ul_start = 65536;
				ul_max = 7340032;
				ul_cnt = 6;
				break;

			case 3:
				ul_start = 131072;
				ul_max = 7340032;
				ul_cnt = 5;
				break;

			case 4:
				ul_start = 262144;
				ul_max = 7340032;
				ul_cnt = 4;
				break;

			case 5:
				ul_start = 524288;
				ul_max = 7340032;
				ul_cnt = 3;
				break;

			case 6:
				ul_start = 1048576;
				ul_max = 7340032;
				ul_cnt = 2;
				break;

			default:
				ul_start = 32768;
				ul_max = 7340032;
				ul_cnt = 7;
		}

		int upload_count = std::ceil((double)ul_maxchunkcount / (double)ul_cnt);
		ul_incr = std::ceil((double)(ul_max - ul_start) / (double)upload_count);
		ul_length = ul_length * ul_concurrency * 1000;

	} else return false;

	if ( config.parse("server-config", { "threadcount" }, server_info) &&
		speedtest::xml::all_attributes_found({"threadcount"}, server_info) &&
			is_number(server_info["threadcount"]))
		dl_concurrency = std::stoi(server_info["threadcount"]);
	else return false;

	if ( config.parse("socket-download", { "testlength" }, download_info) &&
		speedtest::xml::all_attributes_found({ "testlength" }, download_info) &&
			is_number(download_info["testlength"])) {

		dl_length = std::stoi(download_info["testlength"]);
		dl_length = dl_length * 1000;

	} else return false;

	this -> _profile = {
		{ // Download
			ul_start, ul_max, ul_incr, bufsize, dl_length, (int)dl_concurrency
		}, { // Upload
			ul_start, ul_max, ul_incr, bufsize, ul_length, (int)ul_concurrency
		} , "from server configuration", "server selected profile" };

	return true;
}

