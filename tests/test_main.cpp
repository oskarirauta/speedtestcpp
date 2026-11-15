// Unit tests for speedtestcpp: no framework, plain checks. Run via `make test`.
// Covers the protocol-version comparison fix (version_ge) and the Client wire
// behaviour (HELLO handshake, version parsing, PING round-trip, --source bind)
// against an in-process fake Ookla server on loopback.

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "speedtest/speedtest.hpp"
#include "speedtest/client.hpp"
#include "speedtest/types.hpp"

static int checks   = 0;
static int failures = 0;

#define CHECK(cond) \
	do { \
		checks++; \
		if ( !(cond) ) { \
			failures++; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " << #cond << std::endl; \
		} \
	} while ( 0 )

// Minimal fake Ookla raw-TCP server: "HI" -> "HELLO <version>", then answers
// each "PING <nanos>" with "PONG <nanos>". Newline-framed, like the protocol.
class FakeOoklaServer {

	private:

		int            _listen_fd = 0;
		int            _port      = 0;
		int            _max_connections;
		std::string    _version;
		std::thread    _thread;
		std::atomic<bool> _running{false};

		bool read_line(int fd, std::string& line) {
			line.clear();
			char c = 0;
			while ( ::read(fd, &c, 1) == 1 ) {
				if ( c == '\n' || c == '\r' )
					return true;
				line += c;
			}
			return false;
		}

		void write_line(int fd, const std::string& line) {
			std::string out = line + "\n";
			::write(fd, out.c_str(), out.size());
		}

		void serve(int fd) {
			std::string line;
			while ( read_line(fd, line) ) {
				if ( line == "HI" )
					write_line(fd, "HELLO " + _version);
				else if ( line.rfind("PING ", 0) == 0 )
					write_line(fd, "PONG " + line.substr(5));
				else
					break;
			}
			::close(fd);
		}

		void run() {
			int served = 0;
			while ( _running && served < _max_connections ) {
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(_listen_fd, &fds);
				timeval tv = { 1, 0 };
				if ( select(_listen_fd + 1, &fds, nullptr, nullptr, &tv) <= 0 )
					continue;
				int fd = ::accept(_listen_fd, nullptr, nullptr);
				if ( fd < 0 )
					continue;
				serve(fd);
				served++;
			}
		}

	public:

		explicit FakeOoklaServer(const std::string& version = "2.11", int max_connections = 16)
				: _max_connections(max_connections), _version(version) {
			_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
			CHECK(_listen_fd > 0);
			int one = 1;
			setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
			sockaddr_in addr = {};
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			addr.sin_port = 0;
			CHECK(bind(_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
			socklen_t len = sizeof(addr);
			getsockname(_listen_fd, (struct sockaddr*)&addr, &len);
			_port = ntohs(addr.sin_port);
			CHECK(listen(_listen_fd, 8) == 0);
			_running = true;
			_thread = std::thread([this] { run(); });
		}

		~FakeOoklaServer() {
			_running = false;
			::close(_listen_fd);
			if ( _thread.joinable() )
				_thread.join();
		}

		int port() const { return _port; }
};

static speedtest::Server local_server(int port) {
	speedtest::Server server;
	server.host = "127.0.0.1:" + std::to_string(port);
	return server;
}

static void test_version_ge() {
	// The regression: "2.11" (major 2, minor 11) must satisfy min "2.3".
	CHECK(speedtest::version_ge("2.11", "2.3"));
	CHECK(speedtest::version_ge("2.3", "2.3"));
	CHECK(speedtest::version_ge("2.10", "2.9"));
	CHECK(speedtest::version_ge("3.0", "2.11"));
	CHECK(speedtest::version_ge("2.3", "2"));
	CHECK(speedtest::version_ge("2.3", ""));

	CHECK(!speedtest::version_ge("2.2", "2.3"));
	CHECK(!speedtest::version_ge("2.9", "2.10"));
	CHECK(!speedtest::version_ge("2.3", "3.0"));
	CHECK(!speedtest::version_ge("2", "2.3"));
	CHECK(!speedtest::version_ge("", "2.3"));

	// A float compare would say 2.11 < 2.3; the component compare must not.
	CHECK(speedtest::version_ge("2.11", "2.3") != (2.11f >= 2.3f));
}

static void test_handshake_and_ping() {
	FakeOoklaServer srv;
	speedtest::Client client(local_server(srv.port()));

	CHECK(client.connect());
	CHECK(client.version_string() == "2.11");
	CHECK(std::abs(client.version() - 2.11f) < 0.001f);

	double ms = 0;
	CHECK(client.ping(ms));
	CHECK(ms >= 0.0 && ms < 1000.0);
}

static void test_handshake_version_parsing() {
	// Older-protocol server: the string must be captured verbatim, not
	// mangled by the float conversion.
	FakeOoklaServer srv("1.9");
	speedtest::Client client(local_server(srv.port()));

	CHECK(client.connect());
	CHECK(client.version_string() == "1.9");
	CHECK(!speedtest::version_ge(client.version_string(), "2.3"));
	CHECK(speedtest::version_ge("1.9", "1.9"));
}

static void test_source_bind() {
	FakeOoklaServer srv;

	// Binding to loopback must succeed.
	speedtest::Client bound(local_server(srv.port()));
	bound.set_source("127.0.0.1");
	CHECK(bound.connect());

	// Binding to an address this host does not own must fail cleanly
	// (no crash, connect reports false).
	speedtest::Client unbound(local_server(srv.port()));
	unbound.set_source("192.0.2.1");
	CHECK(!unbound.connect());
}

int main() {

	test_version_ge();
	test_handshake_and_ping();
	test_handshake_version_parsing();
	test_source_bind();

	if ( failures > 0 ) {
		std::cerr << failures << " of " << checks << " checks FAILED" << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << checks << " checks passed" << std::endl;
	return EXIT_SUCCESS;
}
