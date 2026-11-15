#pragma once

#include <netinet/in.h>
#include <string>

namespace speedtest {

	// Sends the numbered UDP probes of the raw-TCP protocol's packet-loss
	// phase: "LOSS <nounce> <order> <uuid>" datagrams to the server's
	// host:port. Mirrors speedtest-go's transport.PacketLossSender.
	class PacketLossSender {

		public:

			explicit PacketLossSender(const std::string& uuid);
			~PacketLossSender();

			PacketLossSender(PacketLossSender&& other) noexcept;
			PacketLossSender& operator=(PacketLossSender&& other) noexcept;
			PacketLossSender(const PacketLossSender&) = delete;
			PacketLossSender& operator=(const PacketLossSender&) = delete;

			void set_source(const std::string& ip);
			bool connect(const std::string& host);
			bool send(int order);

		private:

			int _fd = 0;
			long long _nounce = 0;
			std::string _uuid;
			std::string _raw;
			std::string _source;
			sockaddr_in _target = {};
	};

}
