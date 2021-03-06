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
	AudioBuffer(uint32_t size = 10240) 
		: _bufferSize(size)
	{
		_buffer.resize(size);
	}

	~AudioBuffer()
	{

	}

	int write(const char *data, uint32_t size)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		uint32_t bytes = writableBytes();

		if (bytes < size)
		{			
			size = bytes;
		}

		if (size > 0)
		{
			memcpy(beginWrite(), data, size);
			_writerIndex += size;
		}

		retrieve(0);
		return size;
	}

	int read(char *data, uint32_t size)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (size > readableBytes())
		{		
			retrieve(0);
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
		return _buffer.size() - _writerIndex;
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
		if (len > 0)
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
		}

		if (_readerIndex > 0 && _writerIndex > 0)
		{
			_buffer.erase(_buffer.begin(), _buffer.begin() + _readerIndex);
			_buffer.resize(_bufferSize);
			_writerIndex -= _readerIndex;
			_readerIndex = 0;		
		}
	}

	void retrieveUntil(const char* end)
	{
		retrieve(end - peek());
	}

	char* begin()
	{
		return &*_buffer.begin();
	}

	const char* begin() const
	{
		return &*_buffer.begin();
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
	uint32_t _bufferSize = 0;
	std::vector<char> _buffer;
	size_t _readerIndex = 0;
	size_t _writerIndex = 0;
};

#endif