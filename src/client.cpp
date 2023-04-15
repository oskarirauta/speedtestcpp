#include <arpa/inet.h>
#include <netdb.h>
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
}

bool speedtest::Client::ping(long &ms) {

	if ( !this -> _fd )
		return false;

	std::stringstream cmd;
	std::string reply;

	auto start = std::chrono::steady_clock::now();
	cmd << "PING " << start.time_since_epoch().count();

	if ( !this -> write(cmd.str()))
        	return false;

	if ( this -> read(reply) && reply.substr(0, 5) == "PONG " ) {
		auto stop = std::chrono::steady_clock::now();
		ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
		return true;
	}

	this -> close();
	return false;
}

bool speedtest::Client::download(const long size, const long chunk_size, long &ms) {

	std::stringstream cmd;
	cmd << "DOWNLOAD " << size;

	if ( !this -> write(cmd.str()))
		return false;

	char *buff = new char[chunk_size];
	for ( size_t i = 0; i < static_cast<size_t>(chunk_size); i++ )
		buff[i] = '\0';

	long missing = 0;
	auto start = std::chrono::steady_clock::now();

	while ( missing != size ) {
		auto current = this -> read(buff, chunk_size);

		if ( current <= 0 ) {
			delete[] buff;
			return false;
		}

		missing += current;
	}

	auto stop = std::chrono::steady_clock::now();
	ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
	delete[] buff;
	return true;
}

bool speedtest::Client::upload(const long size, const long chunk_size, long &ms) {

	std::stringstream cmd;
	cmd << "UPLOAD " << size << "\n";

	auto cmd_len = cmd.str().length();
	char *buff = new char[chunk_size];

	for (size_t i = 0; i < static_cast<size_t>(chunk_size); i++)
		buff[i] = static_cast<char>(rand() % 256);

	long missing = size;
	auto start = std::chrono::steady_clock::now();

	if ( !this -> write(cmd.str())) {
		delete[] buff;
		return false;
	}

	ssize_t w = cmd_len;
	missing -= w;

	while ( missing > 0 ) {

		if ( missing - chunk_size > 0 ) {

			w = this -> write(buff, chunk_size);
			if ( w != chunk_size ) {
				delete[] buff;
				return false;
			}
			missing -= w;

		} else {

			buff[missing - 1] = '\n';
			w = this -> write(buff, missing);

			if ( w != missing ) {
				delete[] buff;
				return false;
			}

			missing -= w;
		}
	}

	std::string reply;

	if ( !this -> read(reply)) {
		delete[] buff;
		return false;
	}

	auto stop = std::chrono::steady_clock::now();

	std::stringstream ss;
	ss << "OK " << size << " ";
	ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
	delete[] buff;

	return reply.substr(0, ss.str().length()) == ss.str();
}

bool speedtest::Client::mk_socket() {

	if ( this -> _fd = socket(AF_INET, SOCK_STREAM, 0); !this -> _fd )
		return false;

	auto hostp = this -> host();
#if __APPLE__
	struct hostent *server = gethostbyname(hostp.first.c_str());
	if ( server == nullptr )
		return false;
#else
	struct hostent server;
	char tmpbuf[BUFSIZ];
	struct hostent *result;
	int errnop;

	if ( gethostbyname_r(hostp.first.c_str(), &server, (char *)&tmpbuf, BUFSIZ, &result, &errnop))
		return false;

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

	return ::connect(this -> _fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) >= 0;
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
		if ( n == -1 )
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
