#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <ostream>

namespace speedtest {

    struct Speed {

        double bytes_per_sec = 0.0;

        double bps()  const { return bytes_per_sec * 8.0; }
        double kbps() const { return bps() / 1000.0; }
        double mbps() const { return bps() / 1000.0 / 1000.0; }

        std::string to_string() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            if ( mbps() >= 1.0 )      { oss << mbps(); return oss.str() + " Mbit/s"; }
            else if ( kbps() >= 1.0 ) { oss << kbps(); return oss.str() + " Kbit/s"; }
            else                      { oss << std::setprecision(0) << bps(); return oss.str() + " bit/s"; }
        }

        friend std::ostream& operator<<(std::ostream& os, const Speed& s) {
            return os << s.to_string();
        }

        static Speed from_bytes_per_sec(double bps) { return { bps }; }
    };

    struct IPInfo {

        std::string ip_address;
        std::string isp;
        std::string country;
        float lat = 0, lon = 0;
    };

    struct Server {

        std::string url;
        std::string name;
        std::string country;
        std::string country_code;
        std::string host;
        std::string sponsor;

        int   id          = 0;
        float lat         = 0;
        float lon         = 0;
        float distance    = 0;
        bool  recommended = false;
    };

}
