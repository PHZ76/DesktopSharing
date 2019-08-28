// 参考librtmp

#ifndef XOP_AMF_OBJECT_H
#define XOP_AMF_OBJECT_H

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <map>
#include <unordered_map>

namespace xop
{

typedef enum
{ 
    AMF0_NUMBER = 0, 
    AMF0_BOOLEAN, 
    AMF0_STRING, 
    AMF0_OBJECT,
    AMF0_MOVIECLIP,		/* reserved, not used */
    AMF0_NULL, 
    AMF0_UNDEFINED, 
    AMF0_REFERENCE, 
    AMF0_ECMA_ARRAY, 
    AMF0_OBJECT_END,
    AMF0_STRICT_ARRAY, 
    AMF0_DATE, 
    AMF0_LONG_STRING, 
    AMF0_UNSUPPORTED,
    AMF0_RECORDSET,		/* reserved, not used */
    AMF0_XML_DOC, 
    AMF0_TYPED_OBJECT,
    AMF0_AVMPLUS,		/* switch to AMF3 */
    AMF0_INVALID = 0xff
} AMF0DataType;

typedef enum
{ 
    AMF3_UNDEFINED = 0,
    AMF3_NULL, 
    AMF3_FALSE, 
    AMF3_TRUE,
    AMF3_INTEGER, 
    AMF3_DOUBLE, 
    AMF3_STRING, 
    AMF3_XML_DOC, 
    AMF3_DATE,
    AMF3_ARRAY, 
    AMF3_OBJECT, 
    AMF3_XML, 
    AMF3_BYTE_ARRAY
} AMF3DataType;
    
typedef enum
{
    AMF_NUMBER,
    AMF_BOOLEAN,
    AMF_STRING,
} AmfObjectType;

struct AmfObject
{  
    AmfObjectType type;

    std::string amf_string;
    double amf_number;
    bool amf_boolean;    

    AmfObject()
    {
        
    }

    AmfObject(std::string str)
    {
       this->type = AMF_STRING; 
       this->amf_string = str; 
    }

    AmfObject(double number)
    {
       this->type = AMF_NUMBER; 
       this->amf_number = number; 
    }
};

typedef std::unordered_map<std::string, AmfObject> AmfObjects;

class AmfDecoder
{
public:    
    int decode(const char *data, int size, int n=-1); //n: 解码次数

    void reset()
    {
        m_obj.amf_string = "";
        m_obj.amf_number = 0;
        m_objs.clear();
    }

    std::string getString() const
    { return m_obj.amf_string; }

    double getNumber() const
    { return m_obj.amf_number; }

    bool hasObject(std::string key) const
    { return (m_objs.find(key) != m_objs.end()); }

    AmfObject getObject(std::string key) 
    { return m_objs[key]; }

    AmfObject getObject() 
    { return m_obj; }

    AmfObjects getObjects() 
    { return m_objs; }
    
private:    
    static int decodeBoolean(const char *data, int size, bool& amf_boolean);
    static int decodeNumber(const char *data, int size, double& amf_number);
    static int decodeString(const char *data, int size, std::string& amf_string);
    static int decodeObject(const char *data, int size, AmfObjects& amf_objs);
    static uint16_t decodeInt16(const char *data, int size);
    static uint32_t decodeInt24(const char *data, int size);
    static uint32_t decodeInt32(const char *data, int size);

    AmfObject m_obj;
    AmfObjects m_objs;    
};

class AmfEncoder
{
public:
    AmfEncoder(uint32_t size = 1024);
    ~AmfEncoder();
     
    void reset()
    {
     m_index = 0;
    }
     
    std::shared_ptr<char> data()
    {
     return m_data;
    }

    uint32_t size() const 
    {
     return m_index;
    }
     
    void encodeString(const char* str, int len, bool isObject=true);
    void encodeNumber(double value);
    void encodeBoolean(int value);
    void encodeObjects(AmfObjects& objs);
    void encodeECMA(AmfObjects& objs);
     
private:
    void encodeInt8(int8_t value);
    void encodeInt16(int16_t value);
    void encodeInt24(int32_t value);
    void encodeInt32(int32_t value); 
    void realloc(uint32_t size);

    std::shared_ptr<char> m_data;    
    uint32_t m_size  = 0;
    uint32_t m_index = 0;
};

}
#endif
