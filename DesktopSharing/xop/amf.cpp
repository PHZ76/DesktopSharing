#include "amf.h"
#include "net/BufferWriter.h"
#include "net/BufferReader.h"

using namespace xop;

int AmfDecoder::decode(const char *data, int size, int n)
{
    int bytesUsed = 0; 
    while (size > bytesUsed)
    {
        int ret = 0;
        char type = data[bytesUsed];
        bytesUsed += 1;     

        switch (type)
        {	
        case AMF0_NUMBER:
            m_obj.type = AMF_NUMBER;     
            ret = decodeNumber(data + bytesUsed, size - bytesUsed, m_obj.amf_number);
            break;

        case AMF0_BOOLEAN:
            m_obj.type = AMF_BOOLEAN;
            ret = decodeBoolean(data + bytesUsed, size - bytesUsed, m_obj.amf_boolean);
            break;

        case AMF0_STRING:
            m_obj.type = AMF_STRING;
            ret = decodeString(data + bytesUsed, size - bytesUsed, m_obj.amf_string);	
            break;

        case AMF0_OBJECT:   
            ret = decodeObject(data + bytesUsed, size - bytesUsed, m_objs);
            break;
            
        case AMF0_OBJECT_END:              
            break;

        case AMF0_ECMA_ARRAY:
            ret = decodeObject(data + bytesUsed + 4, size - bytesUsed - 4, m_objs);
            break;
            
        case AMF0_NULL:
            break;
            
        default:   
            break;
        }                    
        
        if(ret < 0)
        {
            break;
        }
        
        bytesUsed += ret;
        
        n--;
        if(n == 0)
        {
            break;
        }               
    }

	return bytesUsed;
}

int AmfDecoder::decodeNumber(const char *data, int size, double& amf_number)
{
    if (size < 8)
        return 0;

    char *ci = (char*)data;
    char *co = (char*)&amf_number;
    co[0] = ci[7];
    co[1] = ci[6];
    co[2] = ci[5];
    co[3] = ci[4];
    co[4] = ci[3];
    co[5] = ci[2];
    co[6] = ci[1];
    co[7] = ci[0];

    return 8;
}

int AmfDecoder::decodeString(const char *data, int size, std::string& amf_string)
{
    if (size < 2)
    {
        return 0;
    }

    int bytesUsed = 0;
    int strSize = decodeInt16(data, size);
    bytesUsed += 2;
    if (strSize > (size - bytesUsed))
    {
        return -1;
    }

    amf_string = std::string(&data[bytesUsed], 0, strSize);
    bytesUsed += strSize;
    return bytesUsed;
}

int AmfDecoder::decodeObject(const char *data, int size, AmfObjects& amf_objs)
{
    amf_objs.clear();
    int bytesUsed = 0;
    while (size > 0)
    {
        int strLen = decodeInt16(data+ bytesUsed, size);
        size -= 2;
        if (size < strLen)
        {
            return bytesUsed;
        }

        std::string key(data+bytesUsed+2, 0, strLen);
        size -= strLen;

        AmfDecoder dec;
        int ret = dec.decode(data+bytesUsed+2+strLen, size, 1);		
        bytesUsed += 2 + strLen + ret;
        if (ret <= 1)
        {
            break; 
        }
        
        amf_objs.emplace(key, dec.getObject());
    }

    return bytesUsed;
}

int AmfDecoder::decodeBoolean(const char *data, int size, bool& amf_boolean)
{
    if (size < 1)
    {
        return 0;
    }

    amf_boolean = (data[0] != 0);
    return 1;
}

uint16_t AmfDecoder::decodeInt16(const char *data, int size)
{
    uint16_t val = readUint16BE((char*)data);
    return val;
}

uint32_t AmfDecoder::decodeInt24(const char *data, int size)
{
    uint32_t val = readUint24BE((char*)data);
    return val;
}

uint32_t AmfDecoder::decodeInt32(const char *data, int size)
{
    uint32_t val = readUint32BE((char*)data);
    return val;
}

AmfEncoder::AmfEncoder(uint32_t size)
    : m_data(new char[size])
    , m_size(size)
{
    
}

AmfEncoder::~AmfEncoder()
{
    
}

void AmfEncoder::encodeInt8(int8_t value)
{
    if((m_size - m_index) < 1)
    {
        this->realloc(m_size + 1024);
    }

    m_data.get()[m_index++] = value;
}

void AmfEncoder::encodeInt16(int16_t value)
{
    if((m_size - m_index) < 2)
    {
        this->realloc(m_size + 1024);
    }

    writeUint16BE(m_data.get()+m_index, value);
    m_index += 2; 
}

void AmfEncoder::encodeInt24(int32_t value)
{
    if((m_size - m_index) < 3)
    {
        this->realloc(m_size + 1024);
    }

    writeUint24BE(m_data.get()+m_index, value);
    m_index += 3; 
}


void AmfEncoder::encodeInt32(int32_t value)
{
    if((m_size - m_index) < 4)
    {
        this->realloc(m_size + 1024);
    }

    writeUint32BE(m_data.get()+m_index, value);
    m_index += 4; 
}

void AmfEncoder::encodeString(const char *str, int len, bool isObject)
{
    if((int)(m_size - m_index) < (len + 1 + 2 + 2))
    {
        this->realloc(m_size + len + 5);
    }
      
    if (len < 65536)
    {
        if(isObject)
        {
            m_data.get()[m_index++] = AMF0_STRING;
        }
        encodeInt16(len);
    }
    else
    {
        if(isObject)
        {
            m_data.get()[m_index++] = AMF0_LONG_STRING;
        }        
        encodeInt32(len);
    }

    memcpy(m_data.get() + m_index, str, len);
    m_index += len;
}

void AmfEncoder::encodeNumber(double value)
{
    if((m_size - m_index) < 9)
    {
        this->realloc(m_size + 1024);
    }

    m_data.get()[m_index++] = AMF0_NUMBER;	

    char* ci = (char*)&value;
    char* co = m_data.get();
    co[m_index++] = ci[7];
    co[m_index++] = ci[6];
    co[m_index++] = ci[5];
    co[m_index++] = ci[4];
    co[m_index++] = ci[3];
    co[m_index++] = ci[2];
    co[m_index++] = ci[1];
    co[m_index++] = ci[0];
}

void AmfEncoder::encodeBoolean(int value)
{
    if((m_size - m_index) < 2)
    {
        this->realloc(m_size + 1024);
    }

    m_data.get()[m_index++] = AMF0_BOOLEAN;
    m_data.get()[m_index++] = value ? 0x01 : 0x00;
}

void AmfEncoder::encodeObjects(AmfObjects& objs)
{   
    if(objs.size() == 0)
    {
        encodeInt8(AMF0_NULL);
        return ;
    }

    encodeInt8(AMF0_OBJECT);

    for(auto iter : objs)
    {     
        encodeString(iter.first.c_str(), (int)iter.first.size(), false);
        switch(iter.second.type)
        {
            case AMF_NUMBER:
                encodeNumber(iter.second.amf_number);
                break;
            case AMF_STRING:
                encodeString(iter.second.amf_string.c_str(), (int)iter.second.amf_string.size());
                break;
            case AMF_BOOLEAN:
                encodeBoolean(iter.second.amf_boolean);
                break;
            default:
                break;
        }
    }

    encodeString("", 0, false);
    encodeInt8(AMF0_OBJECT_END);
}

void AmfEncoder::encodeECMA(AmfObjects& objs)
{
    encodeInt8(AMF0_ECMA_ARRAY);
    encodeInt32(0);
    
    for(auto iter : objs)
    {     
        encodeString(iter.first.c_str(), (int)iter.first.size(), false);
        switch(iter.second.type)
        {
            case AMF_NUMBER:
                encodeNumber(iter.second.amf_number);
                break;
            case AMF_STRING:
                encodeString(iter.second.amf_string.c_str(), (int)iter.second.amf_string.size());
                break;
            case AMF_BOOLEAN:
                encodeBoolean(iter.second.amf_boolean);
                break;
            default:
                break;
        }
    }

    encodeString("", 0, false);
    encodeInt8(AMF0_OBJECT_END);
}

void AmfEncoder::realloc(uint32_t size)
{
    if(size <= m_size)
    {
        return ;
    }

    std::shared_ptr<char> data(new char[size]);
    memcpy(data.get(), m_data.get(), m_index);
    m_size = size;
    m_data = data;
}
