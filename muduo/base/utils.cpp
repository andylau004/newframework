/*
 * utils.cpp
 *
 *  Created on: May 30, 2014
 *      Author: wanghuan
 */

#include "common.h"
#include "utils.h"

void ErasureUtils::ip2str(uint32_t ipAddress, /* out */ std::string& _ipstr) {
	char buff[INET6_ADDRSTRLEN + 1];
	memset(buff, 0, sizeof(buff));
	if (inet_ntop(AF_INET, &ipAddress, buff, INET6_ADDRSTRLEN) != NULL) {
		_ipstr = buff;
	} else {
		_ipstr = "";
	}
}

uint32_t ErasureUtils::str2ip(const std::string &ipstr)
{
	uint32_t ret = 0;
    // The af argument must be either AF_INET or
    // AF_INET6.  ret is written in network byte order
	if (inet_pton(AF_INET, ipstr.c_str(), &ret) <= 0) {
	    return 0;
	}
	return ret;
}

bool ErasureUtils::is_local_ip4_address(const std::string &ipstr)
{
	struct ifaddrs * ifAddrStruct = NULL;
	struct ifaddrs * ifa = NULL;
	void * tmpAddrPtr = NULL;

	uint32_t ip = str2ip(ipstr);
	getifaddrs(&ifAddrStruct);

	bool found = false;
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr->sa_family == AF_INET) // check it is IP4
		{
			tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			if (0 == memcmp(tmpAddrPtr, &ip, sizeof(uint32_t)))
				found = true;
		}
	}
	if (ifAddrStruct)
		freeifaddrs(ifAddrStruct);

	return found;
}

ErasureFile::ErasureFile()
{
	mFile = NULL;
}

void ErasureFile::open(const std::string &path, const std::string &mode)
{
	close();

	mFile = fopen(path.c_str(), mode.c_str());

	if (mFile == NULL) {
        std::cout/*LOG(ERROR)*/ << "failed to open file " << path << std::endl;
        ErasureUtils::throw_exception(RESULT_ERR_OPEN_FILE_FAILED, "fopen failed");
	}
}

bool ErasureFile::is_open()
{
	return mFile != NULL;
}

ErasureFile::~ErasureFile()
{
	close();
}

int ErasureFile::eof ()
{
	return feof(mFile);
}

void ErasureFile::close()
{
	if (mFile) {
		fclose(mFile);
		mFile = NULL;
	}
}

int32_t ErasureFile::size ()
{
	int32_t cur = ftell(mFile);
	fseek(mFile, 0, SEEK_END);
	int32_t sz = ftell(mFile);
	fseek(mFile, cur, SEEK_SET);
	return sz;
}

void ErasureFile::seek(long int offset, int start)
{
    size_t _n = fseek(mFile, offset, start);
    if (_n != 0){
        std::cout/*LOG(ERROR)*/ << "failed to seek file " << std::endl;
    	ErasureUtils::throw_exception(RESULT_ERR_SEEK_FILE_FAILED, "fseek failed");
    }
}

void ErasureFile::read(void *buf, size_t size)
{
    size_t _n = fread(buf, size, 1, mFile);
    if (_n != 1) {
        std::cout/*LOG(ERROR)*/ << "failed to read file " << std::endl;
        ErasureUtils::throw_exception(RESULT_ERR_DISK_READ_FAILED, "fread failed");
    }
}

void ErasureFile::write(const void *what, size_t size)
{
    size_t _n = fwrite(what, size, 1, mFile);
    if (_n != 1) {
        std::cout/*LOG(ERROR)*/ << "failed to write file " << std::endl;
        ErasureUtils::throw_exception(RESULT_ERR_DISK_WRITE_FAILED, "fwrite failed");
    }
}

void ErasureFile::flush ()
{
	if (mFile) {
		fflush(mFile);
	}
}

bool ErasureFile::exists(const std::string &path)
{
    DIR *dp;
    if ((dp = opendir(path.c_str())) == NULL) {
        return false;
    }
    closedir(dp);
    return true;
//	try
//	{
//		boost::filesystem::path p(path);
//		return boost::filesystem::exists(p);
//	} catch (boost::filesystem::filesystem_error &err) {
//        std::cout/*LOG(ERROR)*/ << "found exception " << err.what() << std::endl;
//		ErasureUtils::throw_exception(err);
//	}
//	return false;
}

void ErasureFile::rename(const std::string &oldpath, const std::string &newpath)
{
//	try
//	{
//		boost::filesystem::path op(oldpath);
//		boost::filesystem::path np(newpath);
//		boost::filesystem::rename(op, np);
//	} catch (boost::filesystem::filesystem_error &err) {
//        std::cout/*LOG(ERROR)*/ << "found exception " << err.what() << std::endl;
//		ErasureUtils::throw_exception(err);
//	}
}



