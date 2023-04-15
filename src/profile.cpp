#include "speedtest/profile.hpp"

const speedtest::Config speedtest::Config::preflight = {
	600000,			// start_size
	2000000,		// max_size
	125000,			// inc_size
	4096,			// buff_size
	10000,			// min_test_time_ms
	2			// Concurrency
};

const speedtest::Profile speedtest::Profile::slowband = {
	{ // Download
		100000,		// start_size
		500000,		// max_size
		10000,		// inc_size
		1024,		// buff_size
		20000,		// min_test_time_ms
		2		// Concurrency
	}, { // Upload
		50000,		// start_size
		80000,		// max_size
		1000,		// inc_size
		1024,		// buff_size
		20000,		// min_test_time_ms
		2		// Concurrency
	},
	"slowband", "Very-slow-line line type"
};

const speedtest::Profile speedtest::Profile::narrowband = {
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
	}, "narrowband", "Buffering-lover line type"
};

const speedtest::Profile speedtest::Profile::broadband = {
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
	}, "broadband", "Broadband line type"
};

const speedtest::Profile speedtest::Profile::fiber = {
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
	}, "fiber", "Fiber / Lan line type"
};

speedtest::Profile::Profile(const double preSpeed) :
		download(speedtest::Profile::slowband.download),
		upload(speedtest::Profile::slowband.upload),
		name(speedtest::Profile::slowband.name),
		description(speedtest::Profile::slowband.description) {

	if ( preSpeed > 4 && preSpeed <= 30 ) {

		this -> download = speedtest::Profile::narrowband.download;
		this -> upload = speedtest::Profile::narrowband.upload;
		this -> name = speedtest::Profile::narrowband.name;
		this -> description = speedtest::Profile::narrowband.description;

	} else if ( preSpeed > 30 && preSpeed < 150 ) {

		this -> download = speedtest::Profile::broadband.download;
		this -> upload = speedtest::Profile::broadband.upload;
		this -> name = speedtest::Profile::broadband.name;
		this -> description = speedtest::Profile::broadband.description;

	} else if ( preSpeed >= 150 ) {

		this -> download = speedtest::Profile::fiber.download;
		this -> upload = speedtest::Profile::fiber.upload;
		this -> name = speedtest::Profile::fiber.name;
		this -> description = speedtest::Profile::fiber.description;

	}
}
