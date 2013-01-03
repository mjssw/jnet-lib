// iocp_buffer.h
#ifndef _IOCP_BUFFER_
#define _IOCP_BUFFER_

#ifdef _MSC_VER
#pragma once
#endif

namespace iocp {


#ifdef _WIN32
struct wsabuf_wrapper: WSABUF
{
	wsabuf_wrapper(char *buffer, size_t length)
	{
		buf = buffer;
		len = static_cast<unsigned long>(length & ULONG_MAX);
	}

	wsabuf_wrapper(const char *buffer, size_t length)
	{
		buf = const_cast<char *>(buffer);
		len = static_cast<unsigned long>(length & ULONG_MAX);
	}

	void shrink_to(size_t s_size)
	{
		if (len > s_size)
			len = static_cast<unsigned long>(s_size & ULONG_MAX);
	}
};
#endif

class const_buffer
{
public:
	const_buffer(const char *buf, size_t len): buf_(buf), len_(len) {}
	void consume(size_t len) { buf_ += len; len_ -= len; }

	const char *raw() { return buf_; }
	size_t len() { return len_; }

#ifdef _WIN32
	operator wsabuf_wrapper() const { return wsabuf_wrapper(buf_, len_); }
#endif

private:
	const char *buf_;
	size_t len_;
};

class mutable_buffer
{
public:
	mutable_buffer(char *buf, size_t size, size_t len)
		: buf_(buf), size_(size), len_(len) {}
	void append(size_t len) { buf_ += len; len_ += len; }

	char *raw_pos() { return buf_; }
	size_t remain() { return size_ - len_; }
	size_t len() { return len_; }

#ifdef _WIN32
	operator wsabuf_wrapper() const { return wsabuf_wrapper(buf_, size_ - len_); }
#endif

	private:
	char *buf_;
	size_t size_;
	size_t len_;
};

}

#endif
