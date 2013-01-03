#include "fiber_file.h"
#include <iostream>

static char to_write[] = "jeff is a good man00\r\n";
static char read_buf[1024] = {0};

static int write_times = 10;

static void file_routine(void *arg)
{
	iocp::fiber_file *file = static_cast<iocp::fiber_file *>(arg);
	iocp::error_code ec;
	ec = file->create("example", true, true);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		return;
	}

	__int64 size = file->length();
	__int64 pos = file->tell();
	printf("file size: %lld @ %lld\n", size, pos);

	for (int i = 0; i < write_times; ++i) {
		size_t bytes_written;
		sprintf(to_write, "jeff is a good man%02d\r\n", i);
		ec = file->write(to_write, sizeof(to_write)-1, bytes_written);
		if (ec) {
			std::cout << ec.to_string() << std::endl;
			file->close();
			return;
		}
	}

	file->close();

	ec = file->open("example", false);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		return;
	}
	size = file->length();
	pos = file->tell();
	printf("file size: %lld @ %lld\n", size, pos);

	for (;;) {
		size_t bytes_read;
		ec = file->read_line(read_buf, 1024, bytes_read, "\r\n", false);
		if (ec) {
			std::cout << ec.to_string() << std::endl;
			break;
		}
		printf("%.*s\n", bytes_read, read_buf);
	}

	file->close();
}

void fiber_file_test()
{
	fiber::scheduler::convert_thread(0);
	iocp::service svr;
	iocp::fiber_file file(svr);
	file.invoke(file_routine, &file);

	iocp::error_code ec;
	size_t result = svr.run(ec);

	std::cout << "finished " << result << " operations" << std::endl;
	std::cout << ec.to_string() << std::endl;
}
