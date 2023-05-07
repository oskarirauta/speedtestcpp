#include <cmath>
#include "speedtest/speedtest.hpp"

static const float EARTH_RADIUS_KM = 6371.0;

std::string getAttributeValue(const std::string& data, const size_t offset, const size_t max_pos, const std::string& attribute_name) {

        size_t pos = data.find(attribute_name + "=\"", offset);

        if (( pos == std::string::npos ) || ( pos >= max_pos ))
                return "";

        size_t value_pos = pos + attribute_name.length() + 2;
        size_t end = data.find("\"", value_pos);

        return data.substr(pos + attribute_name.length() + 2, end - value_pos);
}

template<typename T>
T deg2rad(T n) {
	return (n * M_PI / 180);
}

template<typename T>
T harversine(std::pair<T, T> n1, std::pair<T, T> n2) {

	T lat1r = deg2rad(n1.first);
	T lon1r = deg2rad(n1.second);
	T lat2r = deg2rad(n2.first);
	T lon2r = deg2rad(n2.second);
	T u = std::sin((lat2r - lat1r) / 2);
	T v = std::sin((lon2r - lon1r) / 2);
	return 2.0 * EARTH_RADIUS_KM * std::asin(std::sqrt(u * u + std::cos(lat1r) * std::cos(lat2r) * v * v));
}

bool speedtest::SpeedTest::parse_servers(const std::string xml, speedtest::IPInfo& info, std::vector<std::map<std::string, std::string>> &servers) {

	std::string data(xml);
	const std::string server_tag_start = "<server ";
	std::vector<std::map<std::string, std::string>> arr;

	size_t server_tag_begin = data.find(server_tag_start);
	while ( server_tag_begin != std::string::npos ) {

		size_t server_tag_end = data.find("/>", server_tag_begin);
		std::map<std::string, std::string> pairs;

		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "name"); !value.empty())
			pairs["name"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "url"); !value.empty())
			pairs["url"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "country"); !value.empty())
			pairs["country"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "cc"); !value.empty())
			pairs["cc"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "host"); !value.empty())
			pairs["host"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "sponsor"); !value.empty())
			pairs["sponsor"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "id"); !value.empty())
			pairs["id"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "lat"); !value.empty())
			pairs["lat"] = value;
		if ( std::string value = getAttributeValue(data, server_tag_begin, server_tag_end, "lon"); !value.empty())
			pairs["lon"] = value;

		if ( pairs.contains("url") && pairs.contains("lat") && pairs.contains("lon") && !pairs["url"].empty()) {

			float lat = std::stof(pairs["lat"]);
			float lon = std::stof(pairs["lon"]);
			float distance = harversine(std::make_pair(info.lat, info.lon), std::make_pair(lat, lon));
			pairs["distance"] = std::to_string(distance);
		}

		if ( !pairs.empty()) {

			if ( !pairs.contains("distance"))
				pairs["distance"] = "0";

			arr.push_back(pairs);
			pairs.clear();
		}

                server_tag_begin = data.find(server_tag_start, server_tag_begin + 1);
        }

	for ( auto const& elem : arr ) {

		if ( elem.contains("name") && elem.contains("url") && elem.contains("host") && elem.contains("id") &&
			elem.contains("lat") && elem.contains("lon") && elem.contains("distance") &&
			elem.contains("country") && elem.contains("cc") && elem.contains("sponsor"))
			servers.push_back(elem);
	}

	return !servers.empty();
}
