# SpeedTestCpp

Yet another unofficial speedtest.net client cli interface

It supports the new (undocumented) raw TCP protocol for better accuracy.
Based on taganaka's good work at https://github.com/taganaka/SpeedTest

## Features

1. Automatic server selection, unless manual selection desired or
   best server discovery based on speed and distance from you is wanted.

2. Automatic profile, unless unavailable or not desired, in that case;
   Line type discovery to select the best test profile based on your line speed.

3. Aggressive multi-threading program in order to saturate your bandwidth quickly.

4. Test supported: Ping / Jitter / Download speed / Upload speed / Packet loss (UDP).

5. Provide a URL to the speedtest.net share results image using option --share

## Installing

### Requirements

1. A modern C++ compiler
2. make
3. libcurl

### Building

```
$ make
```

to build statically linked, provide `SPEEDTEST_LINK_SHARED=no` environment variable for make.
```
$ SPEEDTEST_LINK_SHARED=no make
```

Built default targets are static and shared library, speedtest cli program and minitest.
Install process is manual, where you copy shared library to your shared library path with necessary
symlinks unless you built static targets. Next copy provided binaries to your bin path.

## Usage

```
$ ./speedtest --help
SpeedTest++ version 1.20.0
Speedtest.net command line interface
Info: https://github.com/oskarirauta/speedtestcpp
Author: Francesco Laurita <francesco.laurita@gmail.com>
Co-authored-by: Oskari Rauta <oskari.rauta@gmail.com>

Usage: ./SpeedTest  [--latency] [--quality] [--download] [--upload] [--share] [--help]
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

$
```

## License

SpeedTest++ is available as open source program under the terms of the [MIT License](http://opensource.org/licenses/MIT).
