/*
 * errcode.h
 *
 *  Created on: May 7, 2014
 *      Author: wanghuan
 */

#ifndef ERRCODE_H_
#define ERRCODE_H_

#define RESULT_IS_SUCCESS(code) (code>=0)
#define RESULT_IS_FAILURE(code) (code<0)

#define MFS_RETRIABLE_INDICATOR (-9)
#define MFS_SYSTEM_ERROR_LIMIT (-10000)

//
// Common ERROR Codes
//
static const int RESULT_SUCCESS                                 = 0;
static const int RESULT_ERR_GENERAL                             = -1;
static const int RESULT_ERR_INVALID_ARGUMENT                    = -2;   // bad value for input parameter
static const int RESULT_ERR_INVALID_STATE                       = -3;
static const int RESULT_ERR_VER_INCORRECT                       = -4;   // the version is incorrect
static const int RESULT_ERR_TOKEN_INCORRECT                     = -5;   // the token is incorrect
static const int RESULT_ERR_NOT_INITIALIZED                     = -6;   // the object is not initialized
static const int RESULT_ERR_PROG_BUG							= -7;

static const int RESULT_ERR_NOT_SUPPORTED                       = -998; // The operation is not support yet
static const int RESULT_ERR_RESOURCE_INSUFFICIENT               = -999; // The resource is not enough to response any request

// Common Error Codes
static const int RESULT_ERR_DIRECTORY_NOT_EXIST							= -101;
static const int RESULT_ERR_OPEN_FILE_FAILED							= -102;
static const int RESULT_ERR_PRE_ALLOCATE_FAILED							= -103;
static const int RESULT_ERR_SEEK_FILE_FAILED							= -104;
static const int RESULT_ERR_WRITE_FILE_FAILED							= -105;
static const int RESULT_ERR_FILE_NOT_EXIST								= -106;
static const int RESULT_ERR_PART_NOT_EXIST								= -107;
static const int RESULT_ERR_DISK_WRITE_FAILED							= -111;
static const int RESULT_ERR_DISK_READ_FAILED							= -112;
static const int RESULT_ERR_DISK_IO_FAILED								= -113;
static const int RESULT_ERR_WRONG_OPEN_MODE								= -114;
static const int RESULT_ERR_WRONG_OPEN_TYPE								= -115;
static const int RESULT_ERR_DISK_FILESYSTEM_ERROR						= -116;

//thrift error
static const int RESULT_ERR_THRIFT_CONNECT_FAILED						= -201;
static const int RESULT_ERR_THRIFT_GET_TOKEN_FAILED						= -202;
static const int RESULT_ERR_THRIFT_GET_NODEINFO_FAILED					= -203;

//datanode error
static const int RESULT_DATANODE_ERR_MAINTAINENCE						= -301;
static const int RESULT_ERR_INSUFFICIENT_DATANODE						= -302;
static const int RESULT_ERR_INSUFFICIENT_RECOVERNODE					= -303;
static const int RESULT_ERR_DATANODE_NOT_FOUND							= -304;
static const int RESULT_ERR_RECOVERY_NOT_FOUND							= -305;
static const int RESULT_RECOVERY_TOKEN_NOT_FOUND						= -306;
static const int RESULT_ERR_NO_AVAILABLE_DISK_FOUND						= -307;
static const int RESULT_ERR_CHUNK_NOT_FOUND								= -308;
static const int RESULT_ERR_CHUNK_LEN_OVERFLOW							= -309;
static const int RESULT_ERR_DISK_ENTRY_NOT_FOUND						= -310;

static const int RESULT_FILE_IS_LOCAL									= -311;
static const int RESULT_FILE_IS_REMOTE									= -312;

//namenode error
static const int RESULT_NAMENODE_ERR_MAINTAINENCE						= -401;
static const int RESULT_ERR_BLOCK_NOT_FOUND								= -402;

//client error
static const int RESULT_ERR_CLIENT_INVALID_FILENAME						= -601;
static const int RESULT_ERR_FILE_SIZE_OVERFLOW							= -602;
static const int RESULT_ERR_BLOCK_FILE_BROKEN							= -603;

#endif /* ERRCODE_H_ */
