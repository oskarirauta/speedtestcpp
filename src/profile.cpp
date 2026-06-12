#include "speedtest/profile.hpp"

const speedtest::Config speedtest::Config::preflight = {
	600000,			// start_size
	2000000,		// max_size
	125000,			// inc_size
	4096,			// buff_size
	10000,			// min_test_time_ms
	2			// Concurrency
};

speedtest::Profile speedtest::Profile::uninitialized() {

	return speedtest::Profile(
	{ /* Download */ -1, -1, -1, -1, -1, -1 },
	{ /* Upload   */ -1, -1, -1, -1, -1, -1 },
	"", "");
};

speedtest::Profile speedtest::Profile::slowband(speedtest::Speed preSpeed) {

	speedtest::Profile p(
	{ // Download
		8000,   // start_size
		500000, // max_size
		8000,   // incr_size
		1024,   // buff_size
		20000,  // min_test_time_ms
		1       // concurrency
	}, { // Upload
		4000,   // start_size
		250000, // max_size
		4000,   // incr_size
		1024,   // buff_size
		20000,  // min_test_time_ms
		1       // concurrency
	}, "slowband", "Very slow line type");

	if ( preSpeed.bytes_per_sec > 0 ) {
		// Scale segment sizes so each segment takes ~1 second at measured speed,
		// giving smooth live updates even on very slow links (~20 kbit/s).
		long dl_block = std::max(static_cast<long>(preSpeed.bytes_per_sec), 4000L);
		long ul_block = std::max(dl_block / 2, 2000L);

		p.download.start_size = dl_block;
		p.download.incr_size  = dl_block / 4;
		p.download.max_size   = dl_block * 32;

		p.upload.start_size = ul_block;
		p.upload.incr_size  = ul_block / 4;
		p.upload.max_size   = ul_block * 32;
	}

	return p;
};

speedtest::Profile speedtest::Profile::narrowband() {

	return speedtest::Profile(
	{ // Download
		1000000,	// start_size
		100000000,	// max_size
		750000,		// inc_size
		4096,		// buff_size
		20000,		// min_test_time_ms
		2		// Concurrency
	}, { // Upload
		1000000,	// start_size
		100000000,	// max_size
		550000,		// inc_size
		4096,		// buff_size
		20000,		// min_test_time_ms
		2		// Concurrency
	}, "narrowband", "Buffering-lover line type");
};

speedtest::Profile speedtest::Profile::broadband() {

	return speedtest::Profile(
	{ // Download
		1000000,	// start_size
		100000000,	// max_size
		750000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		32		// concurrency
	}, { // Upload
		2000000,	// start_size
		70000000,	// max_size
		400000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		12		// concurrency
	}, "broadband", "Broadband line type");
};

speedtest::Profile speedtest::Profile::fiber() {

	return speedtest::Profile(
	{ // Download
		5000000,	// start_size
		120000000,	// max_size
		950000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		32		// concurrency
	}, { // Upload
		4000000,	// start_size
		70000000,	// max_size
		500000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		16		// concurrency
	}, "fiber", "Fiber / Lan line type");
};

speedtest::Profile::Profile(speedtest::Speed preSpeed) {

	speedtest::Profile p = [&]() -> speedtest::Profile {
		double mbps = preSpeed.mbps();
		if      ( mbps >= 150 ) return speedtest::Profile::fiber();
		else if ( mbps > 30   ) return speedtest::Profile::broadband();
		else if ( mbps > 4    ) return speedtest::Profile::narrowband();
		else                    return speedtest::Profile::slowband(preSpeed);
	}();

	this->download    = p.download;
	this->upload      = p.upload;
	this->name        = p.name;
	this->description = p.description;
}
