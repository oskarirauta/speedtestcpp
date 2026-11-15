#pragma once

#include <ctime>
#include <chrono>
#include <atomic>
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

			explicit Client(const speedtest::Server& server) : _fd(0), _version(-1.0), _version_str(""), _server(server) {}
			~Client();

			Client(Client&& other) noexcept;
			Client& operator=(Client&& other) noexcept;
			Client(const Client&) = delete;
			Client& operator=(const Client&) = delete;

			bool connect();
			bool ping(double &ms);
			bool init_packet_loss(const std::string& id);
			bool packet_loss(speedtest::PacketLoss& result);
			bool upload(long size, long chunk_size, std::atomic<long long>& counter,
			        std::chrono::steady_clock::time_point deadline);
			bool download(long size, long chunk_size, std::atomic<long long>& counter,
			        std::chrono::steady_clock::time_point deadline);
			void close();

			float version();
			const std::string& version_string();
			void set_source(const std::string& ip);
			const std::pair<std::string, int> host();

		private:

			int _fd;
			float _version;
			std::string _version_str;
			std::string _source;
			speedtest::Server _server;

			bool mk_socket();
			bool read(std::string& buffer);
			bool write(const std::string& buffer);
			ssize_t read(void *buf, const long chunk_size);
			ssize_t write(const void *buf, const long chunk_size);

	};

	typedef bool (Client::*opFn)(long size, long chunk_size, std::atomic<long long>& counter,
	        std::chrono::steady_clock::time_point deadline);

}
