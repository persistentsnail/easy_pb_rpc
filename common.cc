#include "common.h"

void ERR_LOG(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char Log_Msg[1024];
	int len = vsnprintf(Log_Msg, sizeof(Log_Msg), fmt, ap);
	va_end(ap);

	printf("Error Log : %s\n", Log_Msg);
}


