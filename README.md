# SpeedTestCpp

Yet another unofficial speedtest.net client cli interface.

It supports the new (undocumented) raw TCP protocol for better accuracy.
Based on taganaka's good work at https://github.com/taganaka/SpeedTest

## Features

1. Automatic server selection — recommended server, or best server by
   latency and distance — unless a server is selected manually.

2. Automatic line-type profiling: a short pre-flight measurement picks
   the test parameters (segment sizes, concurrency) that suit your speed.

3. Aggressive multi-threading to saturate your bandwidth quickly.

4. Tests supported: Ping / Jitter / Download speed / Upload speed.

5. Real-time live speed: per-read, windowed instantaneous rate for a
   smooth display, while the final figure is the accurate average.

6. Shareable speedtest.net result image via `--share`.

7. A reusable library plus four example clients (see below).

## Library

The build produces both a static (`libspeedtestcpp.a`) and a shared
(`libspeedtestcpp.so`) library. Speeds are returned as a `speedtest::Speed`
value that stores bytes/sec internally and converts on demand, so the same
value reads naturally on slow and fast links:

```cpp
speedtest::Speed s = downloadSpeed;
std::cout << s << "\n";   // auto-formats, e.g. "220.45 Mbit/s" or "742.10 Kbit/s"
double mbit = s.mbps();   // 220.45
double kbit = s.kbps();   // 220450.0
double bits = s.bps();    // bits per second
```

## Installing

### Requirements

1. A modern C++ compiler (C++23)
2. make
3. libcurl

### Building

```
$ make
```

Binaries are **statically linked** by default. To build them against the
shared library instead, set `SPEEDTEST_LINK_SHARED=yes`:

```
$ SPEEDTEST_LINK_SHARED=yes make
```

The default targets are the static and shared libraries plus four example
programs: `speedtest`, `minitest`, `bartest` and `sparktest`.

Install is manual: copy the binaries to your `bin` path. If you built the
shared-linked variant, also copy the shared library to your library path
with the necessary symlinks.

## Clients

| Program     | Description                                                            |
|-------------|------------------------------------------------------------------------|
| `speedtest` | Full CLI: latency, jitter, download, upload, `--share`, server picking, and `verbose` / `text` / `json` output. |
| `minitest`  | Minimal (~50 line) example of the smallest possible tester.            |
| `bartest`   | Modern TUI with live filling bar meters and a test-progress bar.       |
| `sparktest` | Modern TUI with a big live figure and a sparkline trend per direction. |

`bartest` and `sparktest` are dependency-free ANSI demos; they auto-select a
server and accept an optional `--insecure` flag and a `host:port` argument to
pin a specific server.

## Usage

```
$ ./speedtest --help
SpeedTest++ version 3.1.0
Speedtest.net command line interface
Info: https://github.com/oskarirauta/speedtestcpp
Author: Francesco Laurita <francesco.laurita@gmail.com>
Co-authored-by: Oskari Rauta <oskari.rauta@gmail.com>
Usage: ./speedtest [--latency] [--download] [--upload] [--share] [--help]
      [--test-server host:port] [--output verbose|text|json]

optional arguments:
  --help                      Show this message and exit
  --list-servers              Show list of servers
  --latency                   Perform latency test only
  --download                  Perform download test only. It includes latency test
  --upload                    Perform upload test only. It includes latency test
  --share                     Generate and provide a URL to the speedtest.net share results image
  --insecure                  Skip SSL certificate verify (Useful for Embedded devices)
  --test-server host:port     Run speed test against a specific server
  --force-by-latency-test     Always select server based on local latency test
  --output verbose|text|json  Set output type. Default: verbose
```

## License

SpeedTest++ is available as open source program under the terms of the [MIT License](http://opensource.org/licenses/MIT).
