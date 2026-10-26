#ifndef _CONSOLE_IO
#define _CONSOLE_IO

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int InitializeUART(unsigned long baudRate);
int ReadUARTN(void* buffer, unsigned long count);
int WriteUARTN(void* buffer, unsigned long count);

int __read_console(__file_handle handle, unsigned char* buffer, size_t* count,
                   __idle_proc idle_proc);
int __write_console(__file_handle handle, unsigned char* buffer, size_t* count,
                    __idle_proc idle_proc);
int __close_console(void);

#ifdef __cplusplus
}
#endif

#endif
