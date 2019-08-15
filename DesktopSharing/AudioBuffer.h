#ifndef AUIDO_BUFFER_H
#define AUIDO_BUFFER_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>  
#include <mutex>

class AudioBuffer
{
public:
	static const uint32_t kInitialSize = 48000 * 8;

	AudioBuffer(uint32_t initialSize = kInitialSize)
		: _buffer(new std::vector<char>(initialSize))
	{
		_buffer->resize(initialSize);
	}

	~AudioBuffer()
	{

	}

	int write(const char *data, uint32_t size)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (writableBytes() < size)
		{
			if (_buffer->size() + size > MAX_BUFFER_SIZE)
			{
				return -1;
			}
			else
			{
				uint32_t bufferSize = _buffer->size();
				_buffer->resize(bufferSize + size);
			}
		}

		memcpy(beginWrite(), data, size);
		_writerIndex += size;
		return size;
	}


	int read(char *data, uint32_t size)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (size > readableBytes())
		{
			return -1;
		}

		memcpy(data, peek(), size);
		retrieve(size);
		return size;
	}

	uint32_t size()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return readableBytes();
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		retrieveAll();
	}

private:

	uint32_t readableBytes() const
	{
		return _writerIndex - _readerIndex;
	}

	uint32_t writableBytes() const
	{
		return _buffer->size() - _writerIndex;
	}

	char* peek()
	{
		return begin() + _readerIndex;
	}

	const char* peek() const
	{
		return begin() + _readerIndex;
	}

	void retrieveAll()
	{
		_writerIndex = 0;
		_readerIndex = 0;
	}

	void retrieve(size_t len)
	{
		if (len <= readableBytes())
		{
			_readerIndex += len;
			if (_readerIndex == _writerIndex)
			{
				_readerIndex = 0;
				_writerIndex = 0;
			}
		}
		else
		{
			retrieveAll();
		}
	}

	void retrieveUntil(const char* end)
	{
		retrieve(end - peek());
	}

	char* begin()
	{
		return &*_buffer->begin();
	}

	const char* begin() const
	{
		return &*_buffer->begin();
	}

	char* beginWrite()
	{
		return begin() + _writerIndex;
	}

	const char* beginWrite() const
	{
		return begin() + _writerIndex;
	}

	std::mutex _mutex;
	std::shared_ptr<std::vector<char>> _buffer;
	size_t _readerIndex = 0;
	size_t _writerIndex = 0;
	static const uint32_t MAX_BUFFER_SIZE = 48000 * 8 * 2;
};

#endif