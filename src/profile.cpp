#include "speedtest/profile.hpp"

const speedtest::Config speedtest::Config::preflight = {
	600000,			// start_size
	2000000,		// max_size
	125000,			// inc_size
	4096,			// buff_size
	10000,			// min_test_time_ms
	2			// Concurrency
};

const speedtest::Profile speedtest::Profile::uninitialized() {

	return speedtest::Profile(
	{ // Download
		-1,		// start_size
		-1,		// max_size
		-1,		// inc_size
		-1,		// buff_size
		-1,		// min_test_time_ms
		-1		// Concurrency
	}, { // Upload
		-1,		// start_size
		-1,		// max_size
		-1,		// inc_size
		-1,		// buff_size
		-1,		// min_test_time_ms
		-1		// Concurrency
	}, "", "");
};

const speedtest::Profile speedtest::Profile::slowband() {

	return speedtest::Profile(
	{ // Download
		8000,		// start_size
		500000,		// max_size
		8000,		// inc_size
		1024,		// buff_size
		20000,		// min_test_time_ms
		1		// Concurrency
	}, { // Upload
		4000,		// start_size
		250000,		// max_size
		4000,		// inc_size
		1024,		// buff_size
		20000,		// min_test_time_ms
		1		// Concurrency
	}, "slowband", "Very-slow-line line type");
};

const speedtest::Profile speedtest::Profile::narrowband() {

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

const speedtest::Profile speedtest::Profile::broadband() {

	return speedtest::Profile(
	{ // Download
		1000000,	// start_size
		100000000,	// max_size
		750000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		32		// concurrency
	}, { // Upload
		1000000,	// start_size
		70000000,	// max_size
		250000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		8		// concurrency
	}, "broadband", "Broadband line type");
};

const speedtest::Profile speedtest::Profile::fiber() {

	return speedtest::Profile(
	{ // Download
		5000000,	// start_size
		120000000,	// max_size
		950000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		32		// concurrency
	}, { // Upload
		1000000,	// start_size
		70000000,	// max_size
		250000,		// inc_size
		65536,		// buff_size
		20000,		// min_test_time_ms
		12		// concurrency
	}, "fiber", "Fiber / Lan line type");
};

speedtest::Profile::Profile(const double preSpeed) {

	if ( preSpeed > 4 && preSpeed <= 30 ) {

		speedtest::Profile narrowband = speedtest::Profile::narrowband();
		this -> download = narrowband.download;
		this -> upload = narrowband.upload;
		this -> name = narrowband.name;
		this -> description = narrowband.description;

	} else if ( preSpeed > 30 && preSpeed < 150 ) {

		speedtest::Profile broadband = speedtest::Profile::broadband();
		this -> download = broadband.download;
		this -> upload = broadband.upload;
		this -> name = broadband.name;
		this -> description = broadband.description;

	} else if ( preSpeed >= 150 ) {

		speedtest::Profile fiber = speedtest::Profile::fiber();
		this -> download = fiber.download;
		this -> upload = fiber.upload;
		this -> name = fiber.name;
		this -> description = fiber.description;

	} else {

		// slowband: very slow link (<= 4 Mbit/s). One static segment size
		// does not work very well on whole range, so we scale up to measured
		// pre-speed so that one segment finishes in approx. 1 second -> realtime
		// reading updates smoothly even on about 20 kbit/s link

		speedtest::Profile slowband = speedtest::Profile::slowband();

		long bytes_per_s = static_cast<long>(preSpeed * 125000.0); // Mbit/s -> bit/s

		long dl_block = bytes_per_s;        // ~1 s load/segmenet
		long ul_block = bytes_per_s / 2;    // upload is usually slower

		if ( dl_block < 4000 ) dl_block = 4000;   // low limits so that segment
		if ( ul_block < 2000 ) ul_block = 2000;   // doesn't reduce to too small

		slowband.download.start_size = dl_block;
		slowband.download.incr_size  = dl_block / 4;    // low ramp
		slowband.download.max_size   = dl_block * 32;   // safety limit

		slowband.upload.start_size = ul_block;
		slowband.upload.incr_size  = ul_block / 4;
		slowband.upload.max_size   = ul_block * 32;

		this -> download = slowband.download;
		this -> upload = slowband.upload;
		this -> name = slowband.name;
		this -> description = slowband.description;
	}
}
