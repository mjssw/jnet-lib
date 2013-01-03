#include "net_file.h"
#include "net_service.h"
#include <iostream>
#include <stdio.h>

#ifdef __linux__
typedef long long __int64;
#endif

static char to_write[] = "jeff is a good man00\r\n";
static char read_buf[1024] = {0};

static int write_times = 10;

void on_read(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
{
	iocp::file *file = static_cast<iocp::file *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		file->close();
		return;
	}
	printf("%.*s\n", (int)bytes_transferred, buf);
	file->asyn_read_line(read_buf, 1024, on_read, file);
}

void on_writen(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	iocp::file *file = static_cast<iocp::file *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		return;
	}
	else if (--write_times) {
		sprintf(to_write, "jeff is a good man%02d\r\n", write_times);
		file->asyn_write(to_write, sizeof(to_write)-1, on_writen, file);
	}
	else {
		iocp::error_code ec;
		file->close();
		ec = file->open("example", false);
		if (ec) {
			std::cout << ec.to_string() << std::endl;
			return;
		}
		__int64 size = file->length();
		__int64 pos = file->tell();
		printf("file size: %lld @ %lld\n", size, pos);
		file->asyn_read_line(read_buf, 1024, on_read, file, "\r\n", true);
	}
}

void file_example()
{
	iocp::service svr;
	iocp::file file(svr);

	iocp::error_code ec = file.create("example", true, true);

	if (ec) {
		std::cout << "00000";
		std::cout << ec.to_string() << std::endl;
		return;
	}

	__int64 size = file.length();
	__int64 pos = file.tell();
	printf("file size: %lld @ %lld\n", size, pos);

	sprintf(to_write, "jeff is a good man%02d\r\n", write_times);
	file.asyn_write(to_write, sizeof(to_write)-1, on_writen, &file);

	size_t result = svr.run(ec);
	
	file.close();

	std::cout << "finished " << result << " operations" << std::endl;
	std::cout << ec.to_string() << std::endl;
}
