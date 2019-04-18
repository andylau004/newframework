/*
 * exception.h
 *
 *  Created on: May 14, 2014
 *      Author: wanghuan
 */

#ifndef EXCEPTION_H_
#define EXCEPTION_H_

#include <string>
#include <sstream>
#include <exception>

class ErasureException : public std::exception
{
public:
	ErasureException(int errcode, const std::string& verbose = "") throw()
	:std::exception(), mErrorCode(errcode), mVerbose(verbose)
    {
        std::ostringstream oss;
        oss << "Error Code: " << mErrorCode << " " << mVerbose;
        mWhat = oss.str();
    }

	virtual ~ErasureException() throw() {}

public: // From std::exception
    virtual const char* what() const throw() { return mWhat.c_str(); }

public:
    int get_error_code() const throw() { return mErrorCode; }

    const std::string& get_verbose() const throw() { return mVerbose; }

protected:
   int mErrorCode;
   std::string mWhat;
   std::string mVerbose;
};


#endif /* EXCEPTION_H_ */
