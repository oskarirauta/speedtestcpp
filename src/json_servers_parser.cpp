#include "speedtest/speedtest.hpp"

enum SP_JSON_PARSE {
	SP_JSON_PARSE_ARRAY, SP_JSON_PARSE_OBJECT, SP_JSON_PARSE_KEY, SP_JSON_PARSE_VALUE, SP_JSON_PARSE_NUMBER
};

bool speedtest::SpeedTest::parse_servers(const std::string json, speedtest::IPInfo& info, std::vector<std::map<std::string, std::string>> &servers) {

	std::string data(json);

	while ( data.front() == ' ' || data.front() == '\t' || data.front() == '\n' ||
		data.front() == '\r' || data.front() == '\f' || data.front() == '\v' )
		data.erase(0, 1);

	while ( data.back() == ' ' || data.back() == '\t' || data.back() == '\n' ||
		data.back() == '\r' || data.back() == '\f' || data.back() == '\v' )
		data.erase(data.size() - 1);

	if ( data.size() < 1 || data.front() != '[' || data.back() != ']' )
		return false;

	data.erase(0, 1);
	data.erase(data.size() - 1);
	servers.clear();

	SP_JSON_PARSE state = SP_JSON_PARSE_ARRAY;
	std::string key, value;
	std::map<std::string, std::string> pairs;
	std::vector<std::map<std::string, std::string>> arr;

	for ( char ch : data ) {

		if ( state == SP_JSON_PARSE_ARRAY && ch == '{' ) {

			state = SP_JSON_PARSE_OBJECT;
			pairs.clear();
			key = "";
			value = "";
		}
		else if ( state == SP_JSON_PARSE_OBJECT && ( ch == ' ' || ch == ':' ))
			state = SP_JSON_PARSE_OBJECT;
		else if ( state == SP_JSON_PARSE_OBJECT && ch == ',' ) {
			key = "";
			value = "";
		}
		else if ( state == SP_JSON_PARSE_OBJECT && ch == '"' && key.empty())
			state = SP_JSON_PARSE_KEY;
		else if ( state == SP_JSON_PARSE_OBJECT && ch == '"' && !key.empty())
			state = SP_JSON_PARSE_VALUE;
		else if ( state == SP_JSON_PARSE_OBJECT && !key.empty() && value.empty() && std::isdigit(ch)) {
			state = SP_JSON_PARSE_NUMBER;
			value = ch;
		}
		else if ( state == SP_JSON_PARSE_OBJECT && ch == '}' ) {

			state = SP_JSON_PARSE_ARRAY;
			if ( pairs.empty())
				continue;

			arr.push_back(pairs);
			pairs.clear();
		}
		else if ( state == SP_JSON_PARSE_KEY && ch == '"' ) {

			state = SP_JSON_PARSE_OBJECT;
			value = "";
		}
		else if ( state == SP_JSON_PARSE_VALUE && ch == '"' ) {

			state = SP_JSON_PARSE_OBJECT;
			if ( !key.empty() && !value.empty())
				pairs[key] = value;

			key = "";
			value = "";
		}
		else if ( state == SP_JSON_PARSE_KEY ) key += ch;
		else if ( state == SP_JSON_PARSE_VALUE ) value += ch;
		else if ( state == SP_JSON_PARSE_NUMBER && std::isdigit(ch)) value += ch;
		else if ( state == SP_JSON_PARSE_NUMBER && !std::isdigit(ch)) {

			state = ch == '}' ? SP_JSON_PARSE_ARRAY : SP_JSON_PARSE_OBJECT;

			if ( !key.empty() && !value.empty())
				pairs[key] = value;

			key = "";
			value = "";

			if ( ch == '}' && !pairs.empty()) {
				arr.push_back(pairs);
				pairs.clear();
			}
		}
	}

	for ( auto const& elem : arr ) {

		if ( elem.contains("name") && elem.contains("url") && elem.contains("host") && elem.contains("id") &&
			elem.contains("lat") && elem.contains("lon") && elem.contains("distance") &&
			elem.contains("country") && elem.contains("cc") && elem.contains("sponsor"))
			servers.push_back(elem);
	}

	return true;
}
