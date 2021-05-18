#ifndef __FLASH_WEAR_LEVELLING_UTILS_H
#define __FLASH_WEAR_LEVELLING_UTILS_H

#include "mbed.h"

#define MEMORY_SIZE_DEFAULT 4096U /* 4KB */
#define PAGE_SIZE_DEFAULT 256U /* 256-Byte */

#define FWL_TAG_INFO(f_, ...) printf("\r\n[FWL ] " f_, ##__VA_ARGS__)

typedef struct __attribute__((packed, aligned(4)))
{
    struct {
        uint8_t* buffer;
        uint32_t addr;
        uint16_t length;
    } data;

    struct {
        uint32_t addr;
        uint32_t crc32;
        uint32_t nextAddr;
        uint32_t prevAddr;
        uint16_t dataLength;
        uint16_t type;
    } header;
} memory_cxt_t;

class FlashWearLevellingCallbacks
{
public:
    typedef enum
    {
        SUCCESS_WRITE,
        SUCCESS_READ,
        ERROR_CRC,
        ERROR_TYPE,
        ERROR_SIZE
    } status_t;

    virtual ~FlashWearLevellingCallbacks();
    virtual bool onRead(uint32_t addr, uint8_t *buff, uint16_t *length);
    virtual bool onWrite(uint32_t addr, uint8_t *buff, uint16_t *length);
    virtual bool onErase(uint32_t page);
    virtual bool onReady();
    virtual void onStatus(status_t s);
};

extern FlashWearLevellingCallbacks defaultCallback;

extern bool calculator_crc32(memory_cxt_t* mem);

template <class varType>
class FlashWearLevellingUtils
{
    typedef enum
    {
        MEMORY_HEADER_TYPE = 0xAA55,
        MEMORY_LENGTH_MAX  = 256,
        MEMORY_HEADER_END  = 0xFFFFFFFF
    } memoryType_t;

public:
    FlashWearLevellingUtils(uint32_t start_addr = 0, size_t memory_size = MEMORY_SIZE_DEFAULT, uint16_t page_size = PAGE_SIZE_DEFAULT);
    ~FlashWearLevellingUtils();
    void setCallbacks(FlashWearLevellingCallbacks *pCallbacks);
    bool begin(bool formatOnFail = false);
    bool format();
    bool write(varType *data);
    bool read(varType *data);
    bool write(uint8_t *buff, uint16_t length);
    bool read(uint8_t *buff, uint16_t length);

private:
    const uint16_t _data_length;
    const uint8_t _header2data_offset_length;
    varType _var;
    size_t _memory_size;
    uint32_t _start_addr;
    uint32_t _current_addr;
    memory_cxt_t _memory_cxt;
    uint16_t _page_size;
    FlashWearLevellingCallbacks *_pCallbacks;
    bool findLastHeader();
    bool getHeader(memory_cxt_t *mem);
    bool getData(memory_cxt_t *mem);
};

template <class varType>
FlashWearLevellingUtils<varType>::
    FlashWearLevellingUtils(uint32_t start_addr,
                            size_t memory_size,
                            uint16_t page_size) : _start_addr(start_addr),
                                                  _memory_size(memory_size),
                                                  _page_size(page_size),
                                                  _header2data_offset_length(12U),
                                                  _data_length(sizeof(varType))
{
    _current_addr = start_addr;
    _pCallbacks = &defaultCallback;
    memset(&_memory_cxt, 0, sizeof(memory_cxt_t));
    _memory_cxt.header.addr = start_addr;
    _memory_cxt.header.prevAddr = start_addr;
    _memory_cxt.header.nextAddr = MEMORY_HEADER_END;
} // FlashWearLevellingUtils

template <class varType>
FlashWearLevellingUtils<varType>::~FlashWearLevellingUtils() {}

/**
 * Find and update current memory header
*/
template <class varType>
bool FlashWearLevellingUtils<varType>::begin(bool formatOnFail)
{
    findLastHeader();
    return true;
} // begin

/**
 * Format memory type as factory
*/
template <class varType>
bool FlashWearLevellingUtils<varType>::format()
{
    return true;
} // begin

template <class varType>
bool FlashWearLevellingUtils<varType>::write(varType *data)
{
    /* todo ... */
    if (_pCallbacks->onReady())
    {
        uint16_t length = sizeof(varType);
        _pCallbacks->onWrite(_current_addr + _header2data_offset_length, (uint8_t *)data, &length);
    }
    return true;
} // write

template <class varType>
bool FlashWearLevellingUtils<varType>::read(varType *data)
{
    /* todo ... */
    if (_pCallbacks->onReady())
    {
        uint16_t length = sizeof(varType);
        _pCallbacks->onRead(_current_addr + _header2data_offset_length, (uint8_t *)data, &length);
    }
    return true;
} // read

template <class varType>
bool FlashWearLevellingUtils<varType>::write(uint8_t *buff, uint16_t length)
{
    /* todo ... */
    if (_pCallbacks->onReady())
    {
        _pCallbacks->onWrite(_current_addr + _header2data_offset_length, buff, &length);
    }
    return true;
} // write

template <class varType>
bool FlashWearLevellingUtils<varType>::read(uint8_t *buff, uint16_t length)
{
    /* todo ... */
    if (_pCallbacks->onReady())
    {
        _pCallbacks->onRead(_current_addr + _header2data_offset_length, buff, &length);
    }
    return true;
} // read

/**
 * @brief Set the callback handlers for this flash memory.
 * @param [in] pCallbacks An instance of a callbacks structure used to define any callbacks for the flash memory.
 */
template <class varType>
void FlashWearLevellingUtils<varType>::setCallbacks(FlashWearLevellingCallbacks *pCallbacks)
{
    if (pCallbacks != nullptr)
    {
        _pCallbacks = pCallbacks;
    }
    else
    {
        _pCallbacks = &defaultCallback;
    }
    FWL_TAG_INFO(">> setCallbacks: 0x%x << setCallbacks", (uint32_t)_pCallbacks);
} // setCallbacks

/**
 * @brief Find last header information.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::findLastHeader()
{
    memory_cxt_t mem_cxt;
    uint32_t header_addr;
    uint32_t find_cnt;

    /* Allocate dynamic memory */
    uint8_t *ptr_data = new (std::nothrow) uint8_t[MEMORY_LENGTH_MAX];
    if (ptr_data == NULL)
    {
        FWL_TAG_INFO("[findLastHeader] Allocate RAM failed!");
        return false;
    }
    FWL_TAG_INFO("[findLastHeader] Allocated %u byte RAM at 0x%x", MEMORY_LENGTH_MAX, (uint32_t)ptr_data);

    /* pointer data buff */
    mem_cxt.data.buffer = ptr_data;

    header_addr = _start_addr;
    find_cnt = 0;
    do {
        /* Update header location address */
        mem_cxt.header.addr = header_addr;

        /* Get header */
        if (!getHeader(&mem_cxt))
        {
            FWL_TAG_INFO("[findLastHeader] Read Header failed!");
            break;
        }

        /* Read data */
        if (!getData(&mem_cxt))
        {
            FWL_TAG_INFO("[findLastHeader] Data failed!");
            break;
        }

        /* Update counter found a header */
        find_cnt++;
        FWL_TAG_INFO("Found %u", find_cnt);

        /* Update last mem_cxt */
        _memory_cxt = mem_cxt;
        /* Update current address */
        _current_addr = header_addr;

        /* Next address to find header */
        header_addr = mem_cxt.header.nextAddr;

        /* Check next address */
        if (MEMORY_HEADER_END == header_addr)
        {
            FWL_TAG_INFO("[findLastHeader] END header");
            break;
        }
    } while(1);

    delete[] ptr_data; /* free memory */
    FWL_TAG_INFO("[findLastHeader] Free %u byte RAM at 0x%x", MEMORY_LENGTH_MAX, (uint32_t)ptr_data);

    if (find_cnt > 0)
    {
        FWL_TAG_INFO("[findLastHeader] Final counter: %u", find_cnt);
        return true;
    }

    FWL_TAG_INFO("[findLastHeader] Not Found header");
    return false;
} // findLastHeader

/**
 * @brief Get data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::getHeader(memory_cxt_t* mem)
{
    uint16_t length;
    /* Get header */
    length = _header2data_offset_length;
    if (!_pCallbacks->onRead(mem->header.addr, (uint8_t *)mem->header.crc32, &length))
    {
        FWL_TAG_INFO("[getHeader] Read Header failed!");
        return false;
    }

    if (length != _header2data_offset_length)
    {
        FWL_TAG_INFO("[getHeader] Read length failed!");
        return false;
    }

    if (mem->header.type != MEMORY_HEADER_TYPE)
    {
        FWL_TAG_INFO("[getHeader] Header type failed!");
        return false;
    }

    if (mem->header.dataLength > MEMORY_LENGTH_MAX)
    {
        FWL_TAG_INFO("[getHeader] Header length failed!");
        return false;
    }

    if ((mem->header.nextAddr != MEMORY_HEADER_END) && (mem->header.nextAddr > (_start_addr + _memory_size)))
    {
        FWL_TAG_INFO("[getHeader] Header nextAddr failed!");
        return false;
    }

#if(1)
    FWL_TAG_INFO("[getHeader] addr 0x%x", mem->header.addr);
    FWL_TAG_INFO("[getHeader] crc32 0x%x", mem->header.crc32);
    FWL_TAG_INFO("[getHeader] header.nextAddr 0x%x", mem->header.nextAddr);
    FWL_TAG_INFO("[getHeader] header.prevAddr 0x%x", mem->header.prevAddr);
    FWL_TAG_INFO("[getHeader] dataLength %u", mem->header.dataLength);
#endif

    return true;
}

/**
 * @brief Get data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::getData(memory_cxt_t* mem)
{
    /* Update data location address */
    mem->data.addr = mem->header.addr + _header2data_offset_length;

    /* Get data */
    mem->data.length = mem->header.dataLength;
    if (!_pCallbacks->onRead(mem->data.addr, mem->data.buffer, &mem->data.length))
    {
        FWL_TAG_INFO("[getData] Read Data failed!");
        return false;
    }

    if (mem->data.length != mem->header.dataLength)
    {
        FWL_TAG_INFO("[getData] Read length failed!");
        return false;
    }

    /* Checksum */
    if(!calculator_crc32(mem))
    {
        FWL_TAG_INFO("[getData] CRC32 header failed!");
        return false;
    }

#if(1)
    FWL_TAG_INFO("[getData] addr 0x%x", mem->data.addr);
    FWL_TAG_INFO("[getData] length %u", mem->data.length);
    FWL_TAG_INFO("[getData] CRC32 succeed!");
#endif

    return true;
}

#endif
