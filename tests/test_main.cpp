// Unit tests for speedtestcpp: no framework, plain checks. Run via `make test`.
// Covers the protocol-version comparison fix (version_ge) and the Client wire
// behaviour (HELLO handshake, version parsing, PING round-trip, --source bind,
// packet-loss probes) against an in-process fake Ookla server on loopback.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <set>
#include <sstream>
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
#include "speedtest/packet_loss_sender.hpp"

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

// Minimal fake Ookla server: raw-TCP protocol on one port ("HI" -> "HELLO
// <version>", PING/PONG, PLOSS counters) plus a UDP listener on the same port
// counting "LOSS <nounce> <order> <uuid>" probes.
class FakeOoklaServer {

	private:

		int            _listen_fd = 0;
		int            _udp_fd    = 0;
		int            _port      = 0;
		int            _max_connections;
		std::string    _version;
		std::thread    _thread;
		std::atomic<bool> _running{false};

		// Packet-loss session state (single-session fake).
		std::string    _loss_uuid;
		int            _loss_sent = 0;
		int            _loss_dup  = 0;
		int            _loss_max  = -1;
		std::set<int>  _loss_seen;
		int            _drop_order = -1;

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

		bool recv_udp() {
			char buf[512];
			sockaddr_in from = {};
			socklen_t from_len = sizeof(from);
			ssize_t n = ::recvfrom(_udp_fd, buf, sizeof(buf) - 1, 0,
			                       (struct sockaddr*)&from, &from_len);
			if ( n <= 0 )
				return false;
			buf[n] = 0;
			std::stringstream ss(buf);
			std::string cmd, uuid;
			long long nounce = 0;
			int order = 0;
			ss >> cmd >> nounce >> order >> uuid;
			if ( cmd != "LOSS" || uuid != _loss_uuid )
				return true;
			if ( order == _drop_order )
				return true;
			_loss_sent++;
			if ( _loss_seen.count(order) )
				_loss_dup++;
			_loss_seen.insert(order);
			if ( order > _loss_max )
				_loss_max = order;
			return true;
		}

		void serve(int fd) {
			std::string line;
			while ( _running ) {
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(fd, &fds);
				FD_SET(_udp_fd, &fds);
				int maxfd = fd > _udp_fd ? fd : _udp_fd;
				timeval tv = { 1, 0 };
				if ( select(maxfd + 1, &fds, nullptr, nullptr, &tv) <= 0 )
					continue;
				if ( FD_ISSET(_udp_fd, &fds) )
					recv_udp();
				if ( FD_ISSET(fd, &fds) ) {
					if ( !read_line(fd, line) )
						break;
					if ( line == "HI" )
						write_line(fd, "HELLO " + _version);
					else if ( line.rfind("HI ", 0) == 0 ) {
						// Real servers reply HELLO again to the loss
						// session greeting — the client must skip it.
						_loss_uuid = line.substr(3);
						write_line(fd, "HELLO " + _version);
					} else if ( line == "INITPLOSS" )
						write_line(fd, "OK");
					else if ( line == "PLOSS" ) {
						// Drain queued probes so the counters are current.
						while ( recv_udp() )
							;
						write_line(fd, "PLOSS " + std::to_string(_loss_sent) + " " +
						           std::to_string(_loss_dup) + " " +
						           std::to_string(_loss_max < 0 ? 0 : _loss_max));
					} else if ( line.rfind("PING ", 0) == 0 )
						write_line(fd, "PONG " + line.substr(5));
					else
						break;
				}
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
			_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
			CHECK(_udp_fd > 0);

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

			// UDP on the same port: the loss probes and the TCP session share
			// it, exactly like a real Ookla server.
			sockaddr_in udp_addr = {};
			udp_addr.sin_family = AF_INET;
			udp_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			udp_addr.sin_port = htons(static_cast<uint16_t>(_port));
			CHECK(bind(_udp_fd, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) == 0);

			// Non-blocking so the PLOSS handler can drain pending probes.
			int flags = fcntl(_udp_fd, F_GETFL, 0);
			fcntl(_udp_fd, F_SETFL, flags | O_NONBLOCK);

			CHECK(listen(_listen_fd, 8) == 0);
			_running = true;
			_thread = std::thread([this] { run(); });
		}

		~FakeOoklaServer() {
			_running = false;
			::close(_listen_fd);
			::close(_udp_fd);
			if ( _thread.joinable() )
				_thread.join();
		}

		void set_drop_order(int order) { _drop_order = order; }
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

static void send_probes(speedtest::PacketLossSender& sender, int count, int extra_order = -1) {
	for ( int i = 0; i < count; i++ )
		CHECK(sender.send(i));
	if ( extra_order >= 0 )
		CHECK(sender.send(extra_order));
}

static void test_packet_loss_clean() {
	FakeOoklaServer srv;
	speedtest::Client client(local_server(srv.port()));
	CHECK(client.connect());
	CHECK(client.init_packet_loss("test-uuid"));

	speedtest::PacketLossSender sender("test-uuid");
	CHECK(sender.connect("127.0.0.1:" + std::to_string(srv.port())));
	send_probes(sender, 40);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	speedtest::PacketLoss pl;
	CHECK(client.packet_loss(pl));
	CHECK(pl.sent == 40);
	CHECK(pl.dup == 0);
	CHECK(pl.max == 39);
	CHECK(pl.LossPercent() == 0.0);
}

static void test_packet_loss_drop() {
	// One probe never arrives: 40 sent, 39 acknowledged, max still 39.
	FakeOoklaServer srv;
	srv.set_drop_order(3);
	speedtest::Client client(local_server(srv.port()));
	CHECK(client.connect());
	CHECK(client.init_packet_loss("test-uuid"));

	speedtest::PacketLossSender sender("test-uuid");
	CHECK(sender.connect("127.0.0.1:" + std::to_string(srv.port())));
	send_probes(sender, 40);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	speedtest::PacketLoss pl;
	CHECK(client.packet_loss(pl));
	CHECK(pl.sent == 39);
	CHECK(pl.dup == 0);
	CHECK(pl.max == 39);
	CHECK(std::abs(pl.LossPercent() - 2.5) < 0.01);
}

static void test_packet_loss_dup() {
	// Order 5 arrives twice: duplicates are acknowledged but are NOT loss.
	FakeOoklaServer srv;
	speedtest::Client client(local_server(srv.port()));
	CHECK(client.connect());
	CHECK(client.init_packet_loss("test-uuid"));

	speedtest::PacketLossSender sender("test-uuid");
	CHECK(sender.connect("127.0.0.1:" + std::to_string(srv.port())));
	send_probes(sender, 40, 5);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	speedtest::PacketLoss pl;
	CHECK(client.packet_loss(pl));
	CHECK(pl.sent == 41);
	CHECK(pl.dup == 1);
	CHECK(pl.max == 39);
	CHECK(pl.LossPercent() == 0.0);
}

static void test_packet_loss_unsupported() {
	// No session, no probes: counters read back zero and Loss is N/A (-1),
	// the contract the dashboard treats as "no loss data".
	FakeOoklaServer srv;
	speedtest::Client client(local_server(srv.port()));
	CHECK(client.connect());

	speedtest::PacketLoss pl;
	CHECK(client.packet_loss(pl));
	CHECK(pl.sent == 0);
	CHECK(pl.dup == 0);
	CHECK(pl.max == 0);
	CHECK(pl.LossPercent() == -1);
}

int main() {

	test_version_ge();
	test_handshake_and_ping();
	test_handshake_version_parsing();
	test_source_bind();
	test_packet_loss_clean();
	test_packet_loss_drop();
	test_packet_loss_dup();
	test_packet_loss_unsupported();

	if ( failures > 0 ) {
		std::cerr << failures << " of " << checks << " checks FAILED" << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << checks << " checks passed" << std::endl;
	return EXIT_SUCCESS;
}
