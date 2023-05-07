#pragma once

#include <string>
#include <vector>
#include <map>

namespace speedtest {

	class xml {

		public:

			explicit xml(const std::string &data);

			const std::string &data();
			bool parse(const std::string &tag, const std::vector<std::string> &attributes, std::map<std::string, std::string> &values);
			bool parse_array(const std::string &tag, const std::vector<std::string> &attributes, std::vector<std::map<std::string, std::string>> &values);

			static bool all_attributes_found(const std::vector<std::string> &attributes, std::map<std::string, std::string> &values);

		private:

			std::string _data;

			const std::string get_attribute(const size_t offset, const size_t max_pos, const std::string& name);
	};

}
