#include "speedtest/xml.hpp"

speedtest::xml::xml(const std::string &data) : _data(data) {}

const std::string &speedtest::xml::data() {
	return this -> _data;
}

bool speedtest::xml::parse(const std::string &tag, const std::vector<std::string> &attributes, std::map<std::string, std::string> &values) {

	std::map<std::string, std::string> pairs;
	size_t tag_begin = this -> _data.find("<" + tag + " ");

	while ( tag_begin != std::string::npos && !speedtest::xml::all_attributes_found(attributes, pairs)) {

		size_t tag_end = this -> _data.find("/>", tag_begin);

		for ( const std::string &attribute : attributes ) {

			if ( pairs.contains(attribute))
				continue;

			if ( std::string value = this -> get_attribute(tag_begin, tag_end, attribute); !value.empty())
				pairs[attribute] = value;
		}

		tag_begin = _data.find("<" + tag + " ", tag_begin + 1);
	}

	for ( auto &pair : pairs )
		values[pair.first] = pair.second;

	return !pairs.empty();
}

bool speedtest::xml::parse_array(const std::string &tag, const std::vector<std::string> &attributes, std::vector<std::map<std::string, std::string>> &values) {

	std::vector<std::map<std::string, std::string>> arr;
	size_t tag_begin = this -> _data.find("<" + tag + " ");

	values.clear();

	while ( tag_begin != std::string::npos ) {

		size_t tag_end = this -> _data.find("/>", tag_begin);
		std::map<std::string, std::string> pairs;

		for ( const std::string &attribute : attributes ) {

			if ( pairs.contains(attribute))
				continue;

			if ( std::string value = this -> get_attribute(tag_begin, tag_end, attribute); !value.empty())
				pairs[attribute] = value;
		}

		tag_begin = _data.find("<" + tag + " ", tag_begin + 1);
	}

	for ( const std::map<std::string, std::string> &pairs : arr )
		values.push_back(pairs);

	return !arr.empty();
}

const std::string speedtest::xml::get_attribute(const size_t offset, const size_t max_pos, const std::string& name) {

	size_t pos = this -> _data.find(name + "=\"", offset);

	if (( pos == std::string::npos ) || ( pos >= max_pos ))
		return "";

	size_t value_pos = pos + name.length() + 2;
	size_t end = this -> _data.find("\"", value_pos);

	return this -> _data.substr(pos + name.length() + 2, end - value_pos);
}

bool speedtest::xml::all_attributes_found(const std::vector<std::string> &attributes, std::map<std::string, std::string> &values) {

	for ( const auto &name : attributes )
		if ( !values.contains(name))
			return false;

	return true;
}
