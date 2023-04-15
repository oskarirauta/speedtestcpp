//
// Created by Francesco Laurita on 6/3/16.
//

#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#include <string>

#  define SHA1 CC_SHA1
#else
#  include <openssl/md5.h>
#  include <openssl/evp.h>
#endif

#include <sstream>
#include <sys/utsname.h>

#include "speedtest/constants.hpp"
#include "speedtest/utils.hpp"

const bool speedtest::hex_digest(const std::string &str, std::string &result) {

	unsigned int sz = EVP_MD_size(EVP_md5());
	unsigned char digest[sz];
	EVP_MD_CTX *ctx;

	if (( ctx = EVP_MD_CTX_create()) == NULL )
		return false;

	if ( EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1 )
		return false;

	if ( EVP_DigestUpdate(ctx, str.c_str(), str.size()) != 1 )
		return false;

	if ( EVP_DigestFinal_ex(ctx, digest, &sz) != 1 )
		return false;

	char hexDigest[33] = {'\0'};
	for (int i = 0; i < 16; i++)
		std::sprintf(&hexDigest[i*2], "%02x", (unsigned int)digest[i]);

	result = std::string(hexDigest);
	return true;
}

const std::string speedtest::user_agent() {

	struct utsname buf;

	if ( uname(&buf))
		return "Mozilla/5.0 Linux-1; U; x86_64; en-us (KHTML, like Gecko) SpeedTestCpp/" + speedtest::version;

	std::stringstream ss;

	ss << "Mozilla/5.0 " << buf.sysname << "-" << buf.release << "; U; " << buf.machine << "; en-us (KHTML, like Gecko) SpeedTestCpp/" + speedtest::version;
	return ss.str();
}
