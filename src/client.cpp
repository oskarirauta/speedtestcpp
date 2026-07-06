#include <cstring>
#include <vector>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "speedtest/client.hpp"

speedtest::Client::~Client() {
	this -> close();
}

bool speedtest::Client::connect() {

	if ( this -> _fd )
		return true;

	if ( !this -> mk_socket())
		return false;

	std::string reply;

	if ( !this -> write("HI")) {
		this -> close();
		return false;
	}

	if ( this -> read(reply)) {

		std::stringstream reply_stream(reply);
		std::string hello;

		reply_stream >> hello >> this -> _version;

		if ( reply_stream.fail()) {
			this -> close();
			return false;
		} else if ( !reply.empty() && hello == "HELLO" )
			return true;
	}

	this -> close();
	return false;
}

void speedtest::Client::close() {

	if ( !this -> _fd )
		return;

	speedtest::Client::write("QUIT");
	::close(this -> _fd);
	this -> _fd = 0;
}

bool speedtest::Client::ping(double &ms) {

	if ( !this -> _fd )
		return false;

	std::stringstream cmd;
	std::string reply;

	auto start = std::chrono::high_resolution_clock::now();
	cmd << "PING " << start.time_since_epoch().count();

	if ( !this -> write(cmd.str()))
        	return false;

	if ( this -> read(reply) && reply.substr(0, 5) == "PONG " ) {
		auto stop = std::chrono::high_resolution_clock::now();
		ms = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() / 1000.0;
		return true;
	}

	this -> close();
	return false;
}

bool speedtest::Client::download(long size, long chunk_size, std::atomic<long long>& counter,
		std::chrono::steady_clock::time_point deadline) {

	std::stringstream cmd;
	cmd << "DOWNLOAD " << size;

	if ( !this -> write(cmd.str()))
		return false;

	std::vector<char> buff(static_cast<size_t>(chunk_size));
	long missing = 0;

	while ( missing < size ) {

		auto current = this -> read(buff.data(), chunk_size);

		if ( current <= 0 )
			return false;

		missing += current;
		counter.fetch_add(current, std::memory_order_relaxed);

		// Stop mid-block once the test window has elapsed; bytes already read
		// are counted, and the caller closes the connection.
		if ( std::chrono::steady_clock::now() >= deadline )
			return true;
	}

	return true;
}

bool speedtest::Client::upload(long size, long chunk_size, std::atomic<long long>& counter,
		std::chrono::steady_clock::time_point deadline) {

	std::stringstream cmd;
	cmd << "UPLOAD " << size << "\n";

	long cmd_len = static_cast<long>(cmd.str().length());
	std::vector<char> buff(static_cast<size_t>(chunk_size));

	for ( size_t i = 0; i < static_cast<size_t>(chunk_size); i++ )
		buff[i] = static_cast<char>(rand() % 256);

	if ( !this -> write(cmd.str()))
		return false;

	long missing = size - cmd_len;
	counter.fetch_add(cmd_len, std::memory_order_relaxed);

	bool aborted = false;

	while ( missing > 0 ) {

		if ( missing - chunk_size > 0 ) {

			ssize_t w = this -> write(buff.data(), chunk_size);
			if ( w != chunk_size )
				return false;
			missing -= w;
			counter.fetch_add(w, std::memory_order_relaxed);

		} else {

			buff[missing - 1] = '\n';
			ssize_t w = this -> write(buff.data(), missing);
			if ( w != missing )
				return false;
			missing -= w;
			counter.fetch_add(w, std::memory_order_relaxed);
		}

		// Stop mid-block once the test window has elapsed. The block is left
		// incomplete, so we skip the OK handshake and let the caller close.
		if ( std::chrono::steady_clock::now() >= deadline ) {
			aborted = true;
			break;
		}
	}

	if ( aborted )
		return true;

	std::string reply;

	if ( !this -> read(reply))
		return false;

	std::stringstream ss;
	ss << "OK " << size << " ";

	return reply.substr(0, ss.str().length()) == ss.str();
}

bool speedtest::Client::mk_socket() {

	if ( this -> _fd = socket(AF_INET, SOCK_STREAM, 0); this -> _fd < 0 ) {
		this -> _fd = 0;
		return false;
	}

	// Disable Nagle so the small per-block trailer ('\n') and UPLOAD command
	// are not held back waiting for a pending ACK. Without this, the
	// Nagle/delayed-ACK interaction stalls ~40ms at every upload block
	// boundary, consistently dragging upload throughput below the link rate.
	int one = 1;
	setsockopt(this -> _fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

	auto hostp = this -> host();
#if __APPLE__
	struct hostent *server = gethostbyname(hostp.first.c_str());
	if ( server == nullptr ) {
		::close(this -> _fd);
		this -> _fd = 0;
		return false;
	}
#else
	struct hostent server;
	char tmpbuf[BUFSIZ];
	struct hostent *result;
	int errnop;

	if ( gethostbyname_r(hostp.first.c_str(), &server, (char *)&tmpbuf, BUFSIZ, &result, &errnop)) {
		::close(this -> _fd);
		this -> _fd = 0;
		return false;
	}
#endif

	int portno = hostp.second;
	struct sockaddr_in serv_addr{};
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

#if __APPLE__
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, (size_t)server->h_length);
#else
	memcpy(&serv_addr.sin_addr.s_addr, server.h_addr, (size_t)server.h_length);
#endif

	serv_addr.sin_port = htons(static_cast<uint16_t>(portno));

	if ( ::connect(this -> _fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 ) {
		::close(this -> _fd);
		this -> _fd = 0;
		return false;
	}

	return true;
}

float speedtest::Client::version() {
    return this -> _version;
}

const std::pair<std::string, int> speedtest::Client::host() {

	std::string targetHost = this -> _server.host;
	std::size_t found = targetHost.find(':');
	std::string host = targetHost.substr(0, found);
	std::string port = targetHost.substr(found + 1, targetHost.length() - found);
	return std::pair<std::string, int>(host, std::atoi(port.c_str()));
}

bool speedtest::Client::read(std::string &buffer) {

	buffer.clear();

	if ( !this -> _fd )
		return false;

	char c;
	while( true ) {

		auto n = this -> read(&c, 1);

		// n <= 0 means the peer closed the connection (0, EOF) or the read
		// errored (-1). ::read never returns < -1, so the old `n < -1` guard
		// never fired: on a throttling server's close the loop spun forever,
		// re-appending the stale `c` and growing `buffer` without bound until
		// the process pinned a core and exhausted all system memory.
		if ( n <= 0 )
			return false;

		if ( c == '\n' || c == '\r' )
			break;

		buffer += c;

	}
	return true;
}

bool speedtest::Client::write(const std::string &buffer) {

	if ( !this -> _fd )
		return false;

	auto len = static_cast<ssize_t >(buffer.length());
	if ( len == 0 )
		return false;

	std::string buff_copy = buffer;

	if ( buff_copy.find_first_of('\n') == std::string::npos ) {
		buff_copy += '\n';
		len += 1;
	}

	auto n = this -> write(buff_copy.c_str(), len);
	return n == len;
}

ssize_t speedtest::Client::read(void *buf, const long chunk_size) {

	if ( !this -> _fd )
		return -1;

	return ::read(this -> _fd, buf, static_cast<size_t>(chunk_size));
}

ssize_t speedtest::Client::write(const void *buf, const long chunk_size) {

	if ( !this -> _fd )
		return -1;

	return ::write(this -> _fd, buf, static_cast<size_t>(chunk_size));
}
