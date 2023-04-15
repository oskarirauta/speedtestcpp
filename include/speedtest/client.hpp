#pragma once

#include <ctime>
#include <chrono>
#include <string>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "speedtest/speedtest.hpp"
#include "speedtest/types.hpp"

namespace speedtest {

	class Client {

		public:

			explicit Client(const speedtest::Server& server) : _fd(0), _version(-1.0), _server(server) {}
			~Client();

			bool connect();
			bool ping(long &ms);
			bool upload(long size, long chunk_size, long &ms);
			bool download(long size, long chunk_size, long &ms);
			void close();

			float version();
			const std::pair<std::string, int> host();

		private:

			int _fd;
			float _version;
			speedtest::Server _server;

			bool mk_socket();
			bool read(std::string& buffer);
			bool write(const std::string& buffer);
			ssize_t read(void *buf, const long chunk_size);
			ssize_t write(const void *buf, const long chunk_size);

	};

	typedef bool (Client::*opFn)(const long size, const long chunk_size, long &ms);

}
