#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "speedtest/packet_loss_sender.hpp"

speedtest::PacketLossSender::PacketLossSender(const std::string& uuid)
		: _uuid(uuid) {

	std::mt19937_64 rng(static_cast<unsigned long long>(
		std::chrono::high_resolution_clock::now().time_since_epoch().count()));
	_nounce = static_cast<long long>(rng() % 10000000000ULL);

	// Template payload; '#' is replaced with the probe order per datagram.
	_raw = "LOSS " + std::to_string(_nounce) + " # " + _uuid;
}

speedtest::PacketLossSender::~PacketLossSender() {
	if ( _fd > 0 )
		::close(_fd);
}

speedtest::PacketLossSender::PacketLossSender(PacketLossSender&& other) noexcept
		: _fd(other._fd), _nounce(other._nounce),
		  _uuid(std::move(other._uuid)), _raw(std::move(other._raw)),
		  _source(std::move(other._source)), _target(other._target) {
	other._fd = 0;
}

speedtest::PacketLossSender& speedtest::PacketLossSender::operator=(PacketLossSender&& other) noexcept {
	if ( this != &other ) {
		if ( _fd > 0 )
			::close(_fd);
		_fd = other._fd; other._fd = 0;
		_nounce = other._nounce;
		_uuid = std::move(other._uuid);
		_raw = std::move(other._raw);
		_source = std::move(other._source);
		_target = other._target;
	}
	return *this;
}

void speedtest::PacketLossSender::set_source(const std::string& ip) {
	_source = ip;
}

bool speedtest::PacketLossSender::connect(const std::string& host) {

	std::string targetHost = host;
	std::size_t found = targetHost.find(':');
	std::string hostname = targetHost.substr(0, found);
	int port = found == std::string::npos ? 8080 : std::atoi(targetHost.substr(found + 1).c_str());

	_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( _fd < 0 ) {
		_fd = 0;
		return false;
	}

	if ( !_source.empty() ) {
		sockaddr_in source_addr = {};
		source_addr.sin_family = AF_INET;
		source_addr.sin_port = 0;
		if ( inet_pton(AF_INET, _source.c_str(), &source_addr.sin_addr) != 1 ||
		     bind(_fd, (struct sockaddr*)&source_addr, sizeof(source_addr)) < 0 ) {
			::close(_fd);
			_fd = 0;
			return false;
		}
	}

	struct addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	struct addrinfo* result = nullptr;
	if ( getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0 || result == nullptr ) {
		::close(_fd);
		_fd = 0;
		return false;
	}

	_target = {};
	_target.sin_family = AF_INET;
	_target.sin_port = htons(static_cast<uint16_t>(port));
	memcpy(&_target.sin_addr, &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr,
	       sizeof(_target.sin_addr));
	freeaddrinfo(result);

	return true;
}

bool speedtest::PacketLossSender::send(int order) {

	if ( _fd <= 0 )
		return false;

	std::string payload = _raw;
	std::size_t marker = payload.find('#');
	if ( marker != std::string::npos )
		payload.replace(marker, 1, std::to_string(order));

	return sendto(_fd, payload.c_str(), payload.size(), 0,
	              (struct sockaddr*)&_target, sizeof(_target)) == static_cast<ssize_t>(payload.size());
}
