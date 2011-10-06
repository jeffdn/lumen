/* ex: set ts=4 noet: */
/* $Id$ */
/* efficiently parse fixed-width binary data by overlaying corresponding struct pointer and then use offsets
 * contained in the header to locate variable-length fields throughout the rest of the message */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../src/include/types.h"
#include "../src/include/endian.h"

#define PROT_FIXED_HEAD		16

#define STRLEN_ALIGNMENT	8 /* start all strings at an offset that is a multiple of this, for speed! */

/* given a strlen + 1 calculate next offset */
#define ALIGN(len)			((len + STRLEN_ALIGNMENT) & ~(STRLEN_ALIGNMENT - 1))

struct prot {
	struct prot_head {
		u32 id, str1len, str2len, str3len;
	} *head;
	char *str1, *str2, *str3;
};

/**
 * dump raw bytes
 */
static void dump(void *v, size_t bytes)
{
	char *c = v;
	while (bytes--) {
		printf((isalnum((int)*c) || isspace((int)*c) ? "%c" : "\\x%02X"), *c);
		c++;
	}
}

/**
 * msg -> buf
 */
size_t packet_encode(char * buf, const struct prot * msg, size_t buflen)
{
	char * orig = buf;
	memcpy(buf, msg->head, PROT_FIXED_HEAD);
	U32_HTN(buf + 0);
	U32_HTN(buf + 4);
	U32_HTN(buf + 8);
	U32_HTN(buf + 12);
	/* adjust string lengths for alignment */
	buf += PROT_FIXED_HEAD;
	memcpy(buf, msg->str1, msg->head->str1len + 1);
	buf += ALIGN(msg->head->str1len + 1);
	memcpy(buf, msg->str2, msg->head->str2len + 1);
	buf += ALIGN(msg->head->str2len + 1);
	memcpy(buf, msg->str3, msg->head->str3len + 1);
	buf += ALIGN(msg->head->str3len + 1);
	return (size_t)(buf - orig);
}


/**
 * buf -> msg
 */
int packet_decode(struct prot * msg, char * buf)
{
	/* overlay all fixed-width binary data */
	msg->head = (struct prot_head *)buf; 
	U32_NTH(&msg->head->id);
	U32_NTH(&msg->head->str1len);
	U32_NTH(&msg->head->str2len);
	U32_NTH(&msg->head->str3len);
	/* assign pointers to all variable-length data */
	/* first var-length begins at end of fixed-length block */
	msg->str1 = buf + PROT_FIXED_HEAD;
	msg->str2 = msg->str1 + ALIGN(msg->head->str1len + 1);
	msg->str3 = msg->str2 + ALIGN(msg->head->str2len + 1);
	return 1;
}

int main(void)
{
	struct prot msg, tmp;
	char bindata[256] =
			"\x01\x00\x00\x00"
			"\x24\x00\x00\x00"
			"\x01\x00\x00\x00"
			"\x4F\x00\x00\x00"
			"some string data in a binary message\x00\x00\x00\x00"
			"A\x00\x00\x00\x00\x00\x00\x00"
			"more zero-terminated string data in a binary message, the best of both worlds?!\x00\x00";
	char shens[256];
	packet_decode(&msg, bindata);
	packet_encode(shens, &msg, sizeof shens);
	packet_decode(&tmp, shens);
	dump(bindata, sizeof bindata);
	printf(
		"\nid=%u, str1len=%u, str2len=%u, str3len=%u\n"
		"str1=\"%s\", str2=\"%s\", str3=\"%s\"\n",
		msg.head->id, msg.head->str1len, msg.head->str2len, 
		msg.head->str3len, msg.str1, msg.str2, msg.str3);
	dump(shens, sizeof shens);
	printf(
		"\nid=%u, str1len=%u, str2len=%u, str3len=%u\n"
		"str1=\"%s\", str2=\"%s\", str3=\"%s\"\n",
		tmp.head->id, tmp.head->str1len, tmp.head->str2len, 
		tmp.head->str3len, tmp.str1, tmp.str2, tmp.str3);
	return 0;
}
:
