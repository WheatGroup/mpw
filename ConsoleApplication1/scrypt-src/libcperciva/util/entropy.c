#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>

#include "warnp.h"

#include "entropy.h"
#include <windows.h>

/**
 * XXX Portability
 * XXX We obtain random bytes from the operating system by opening
 * XXX /dev/urandom and reading them from that device; this works on
 * XXX modern UNIX-like operating systems but not on systems like
 * XXX win32 where there is no concept of /dev/urandom.
 */

/**
 * entropy_read(buf, buflen):
 * Fill the given buffer with random bytes provided by the operating system.
 */
int
entropy_read(uint8_t * buf, size_t len)
{
	HCRYPTPROV provider;
	unsigned __int64 pentium_tsc[1];
	unsigned int i;
	int result = 0;


	if (CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
	{
		result = CryptGenRandom(provider, len, buf);
		CryptReleaseContext(provider, 0);
		if (result)
			return len;
	}

	QueryPerformanceCounter((LARGE_INTEGER *)pentium_tsc);

	for (i = 0; i < 8 && i < len; ++i)
		buf[i] = ((unsigned char*)pentium_tsc)[i];

	return i;
}
