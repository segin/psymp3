#include <stdlib.h> /* for malloc() */
#include <string.h> /* for strlen() */
#include <iconv.h> /* for the iconv() stuff */

unsigned char hex_to_char(unsigned char hex) {
	hex -= 48;
	if(hex <= 9)
		return hex;
	hex -= 8;
	if(hex >= 10 && hex <= 16)
		return hex;
	hex -= 27;
	if(hex >= 10 && hex <= 16)
		return hex;
	return 0;
}

unsigned char *url_to_utf8(unsigned char *url) {
	int i, p = 0, len = strlen(url);
	char *ret = (unsigned char *) malloc(len);
	if(!ret)
		return NULL;
	for(i = 0; url[p]; i++) {
		if(url[p] == '%' && p + 2 < len) {
			ret[i] = (hex_to_char(url[p + 1]) * 0x10) + hex_to_char(url[p + 2]);
			p += 3;
			continue;
		}
		ret[i] = url[p];
		p++;
	}
	ret[i] = 0;
	return ret;
}

wchar_t *utf8_to_utf16(unsigned char *str) {
	size_t inbytesleft = strlen(str), outbytesleft = inbytesleft * 2;
	char *ret = malloc(outbytesleft);
	iconv_t cd = iconv_open("UTF-8", "UTF16");
	iconv(cd, (const char **) &str, &inbytesleft, &ret, &outbytesleft);
	iconv_close(cd);
	return (wchar_t *) ret;
}