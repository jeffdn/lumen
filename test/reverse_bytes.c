/* ex: set ts=4 noet: */
/* $Id $ */

#include <assert.h>
#include "../src/include/types.h"
#include "../src/include/endian.h"

int main(void)
{
	u16 sixteen		= 0x0123;
	u32 thirtytwo	= 0x01234567UL;
	u64 sixtyfour	= 0x0123456789ABCDEFULL;
	U16_REVERSEBYTES(&sixteen);
	U32_REVERSEBYTES(&thirtytwo);
	U64_REVERSEBYTES(&sixtyfour);
	printf("u64 reversed:0x%llX\n", sixtyfour);
	assert(sixteen == 0x2301);
	assert(thirtytwo == 0x67452301UL);
	assert(sixtyfour == 0xEFCDAB8967452301ULL);
	return 0;
}

