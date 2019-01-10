/*
 * utils.h
 *
 *  Created on: May 14, 2014
 *      Author: wanghuan
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <string>
#include <boost/crc.hpp>
#include <boost/filesystem.hpp>

#include "errcode.h"
#include "exception.h"



class ErasureUtils
{
public:
	typedef boost::crc_optimal<8, 0x9B, 0, 0, false, false>  Crc8Type;

    static inline void throw_exception(const int errorcode, const std::string& verbose) throw (ErasureException) {
    	throw ErasureException(errorcode, verbose);
    }

    static inline void throw_exception(const int errorcode) throw (ErasureException) {
		throw ErasureException(errorcode, "");
	}

    static inline void throw_exception(boost::filesystem::filesystem_error& err) throw (ErasureException) {
        std::ostringstream verbose;
        verbose << err.code() << ". " << err.what();
        throw ErasureException(RESULT_ERR_DISK_FILESYSTEM_ERROR, verbose.str());
    }

    static inline void throw_exception(std::ios_base::failure &err) throw (ErasureException) {
    	throw ErasureException(RESULT_ERR_DISK_IO_FAILED, err.what());
    }

    static inline void rethrow (ErasureException &err) throw (ErasureException) {
    	throw err;
    }

    static uint32_t parse_int(const std::string &str) {
    	return atoi(str.c_str());
    }

    static void ip2str(uint32_t ipAddress, /* out */ std::string& _ipstr);

    static uint32_t str2ip(const std::string &ipstr);

    static bool is_local_ip4_address(const std::string &ipstr);

    static char crc_8(char* buffer, int32_t length) {
        Crc8Type crc8helper;
        crc8helper.process_bytes(buffer, length);
        return crc8helper.checksum();
    }
};

class ErasureFile
{
public:
	ErasureFile ();

	~ErasureFile ();

    void open(const std::string &path, const std::string &mode);

    bool is_open ();

    int eof ();

    void close();

    void seek(long int offset, int start);

    void read(void *buf, size_t size);

    void write(const void *what, size_t size);

    void flush ();

    int32_t size ();

    static bool exists(const std::string &path);

    static void rename(const std::string &oldpath, const std::string &newpath);

private:
	FILE *mFile;
};

#endif /* UTILS_H_ */
