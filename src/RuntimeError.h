#ifndef _RUNTIME_ERROR_
#define _RUNTIME_ERROR_

#include <errno.h>
#include <string.h>

struct RuntimeError : public std::runtime_error
{
	RuntimeError(std::string msg)
		: std::runtime_error(msg + strerror(errno)) {
	}
};

#endif  // _RUNTIME_ERROR_

