#ifndef __FLASH_WEAR_LEVELLING_UTILS_H
#define __FLASH_WEAR_LEVELLING_UTILS_H

#include "mbed.h"

#define MEMORY_SIZE_DEFAULT 4096U /* 4KB */
#define PAGE_SIZE_DEFAULT 256U /* 256-Byte */

#define FWL_TAG_INFO(f_, ...) printf("\r\n[FWL ] " f_, ##__VA_ARGS__)

typedef struct __attribute__((packed, aligned(4)))
{
    struct {
        uint8_t* pBuffer;
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
    virtual bool onErase(uint32_t addr, uint16_t length);
    virtual bool onReady();
    virtual void onStatus(status_t s);
};

extern FlashWearLevellingCallbacks defaultCallback;

extern uint32_t calculator_crc32(memory_cxt_t* mem);

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
    bool write(uint8_t *buff, uint16_t *length);
    bool read(uint8_t *buff, uint16_t *length);

private:
    const uint16_t _data_length;
    const uint8_t _header2data_offset_length;
    varType _var;
    size_t _memory_size;
    uint32_t _start_addr;
    uint32_t _current_header_addr;
    memory_cxt_t _memory_cxt;
    uint16_t _page_size;
    FlashWearLevellingCallbacks *_pCallbacks;
    bool findLastHeader();
    bool getHeader(memory_cxt_t *mem);
    bool setHeader(memory_cxt_t *mem);
    bool getData(memory_cxt_t *mem);
    bool setData(memory_cxt_t *mem);
    bool eraseData(uint32_t addr, uint16_t length);
    bool writeOut(uint32_t addr, uint8_t* buff, uint16_t* length);
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
    _current_header_addr = start_addr;
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
    uint16_t length = sizeof(varType);
    if (write((uint8_t *)data, &length))
    {
        if (length == sizeof(varType))
        {
            return true;
        }
    }

    return false;
} // write

template <class varType>
bool FlashWearLevellingUtils<varType>::read(varType *data)
{
    uint16_t length = sizeof(varType);
    if (read((uint8_t *)data, &length))
    {
        if (length == sizeof(varType))
        {
            return true;
        }
    }

    return false;
} // read

template <class varType>
bool FlashWearLevellingUtils<varType>::write(uint8_t *buff, uint16_t *length)
{
    memory_cxt_t w_memory = _memory_cxt;
    w_memory.data.pBuffer = buff;
    w_memory.data.length  = *length;
    if (setData(&w_memory))
    {
        _memory_cxt = w_memory;
        return true;
    }
    return false;
} // write

template <class varType>
bool FlashWearLevellingUtils<varType>::read(uint8_t *buff, uint16_t *length)
{
    if ((*length) < _memory_cxt.header.dataLength)
    {
        FWL_TAG_INFO("[read] length buffer is not enough!");
        return false;
    }
    /* Read data */
    _memory_cxt.data.pBuffer = buff;
    if (!getData(&_memory_cxt))
    {
        FWL_TAG_INFO("[read] Data failed!");
        return false;
    }

    *length = _memory_cxt.header.dataLength;
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
    uint32_t find_cnt;

    /* Allocate dynamic memory */
    uint8_t *ptr_data = new (std::nothrow) uint8_t[MEMORY_LENGTH_MAX];
    if (ptr_data == NULL)
    {
        FWL_TAG_INFO("[findLastHeader] Allocate RAM failed!");
        return false;
    }
    FWL_TAG_INFO("[findLastHeader] Allocated %u byte RAM at 0x%x", MEMORY_LENGTH_MAX, (uint32_t)ptr_data);

    mem_cxt.header.nextAddr = _start_addr;
    find_cnt = 0;
    do {
        /* End */
        if (MEMORY_HEADER_END == mem_cxt.header.addr)
        {
            FWL_TAG_INFO("[findLastHeader] Header Addr End!");
            break;
        }

        /* Get header */
        mem_cxt.header.addr = mem_cxt.header.nextAddr;
        if (!getHeader(&mem_cxt))
        {
            FWL_TAG_INFO("[findLastHeader] Read Header failed!");
            break;
        }

        /* Read data */
        mem_cxt.data.pBuffer = ptr_data;
        if (!getData(&mem_cxt))
        {
            FWL_TAG_INFO("[findLastHeader] Data failed!");
            break;
        }

        /* Update counter found a header */
        find_cnt++;
        FWL_TAG_INFO("Found %u", find_cnt);

        /* Save last mem_cxt */
        _memory_cxt = mem_cxt;
    } while(1);

    delete[] ptr_data; /* free memory */
    FWL_TAG_INFO("[findLastHeader] Free %u byte RAM at 0x%x", MEMORY_LENGTH_MAX, (uint32_t)ptr_data);

    if (find_cnt > 0)
    {
        FWL_TAG_INFO("[findLastHeader] Final counter: %u", find_cnt);
#if(1)
        FWL_TAG_INFO("[findLastHeader] addr 0x%x", _memory_cxt.header.addr);
        FWL_TAG_INFO("[findLastHeader] crc32 0x%x", _memory_cxt.header.crc32);
        FWL_TAG_INFO("[findLastHeader] header.nextAddr 0x%x", _memory_cxt.header.nextAddr);
        FWL_TAG_INFO("[findLastHeader] header.prevAddr 0x%x", _memory_cxt.header.prevAddr);
        FWL_TAG_INFO("[findLastHeader] dataLength %u", _memory_cxt.header.dataLength);
#endif
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

    if (MEMORY_HEADER_END == mem->header.addr)
    {
        FWL_TAG_INFO("[getHeader] Header Addr End!");
        return false;
    }
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

    if (0xFFFFFFFF == mem->header.crc32)
    {
        FWL_TAG_INFO("[getHeader] Header CRC32 failed!");
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
 * @brief set header into flash memory from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 * @Notify data buffer and data length must be available before call setHeader()
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::setHeader(memory_cxt_t* mem)
{
    memory_cxt_t temp = *mem;

    if (mem->data.pBuffer == NULL)
    {
        FWL_TAG_INFO("[setHeader] Data buffer NULL");
        return false;
    }

    if (mem->data.length > MEMORY_LENGTH_MAX)
    {
        FWL_TAG_INFO("[setHeader] Data length failed!");
        return false;
    }

    /* Make header prev address */
    mem->header.prevAddr = temp.header.addr;
    /* Make header address */
    mem->header.addr = temp.header.nextAddr;
    /* Make header next address */
    mem->header.nextAddr = temp.header.nextAddr;
    mem->header.nextAddr += _header2data_offset_length;
    mem->header.nextAddr += temp.data.length;
    /* the space remain memory is not enough fill data */
    if(mem->header.nextAddr > (_start_addr + _memory_size))
    {
        FWL_TAG_INFO("[setHeader] Reset address");
        /* Reset address header base equal _start_addr */
        mem->header.addr = _start_addr;
        /* Make header next address again */
        mem->header.nextAddr = _start_addr;
        mem->header.nextAddr += _header2data_offset_length;
        mem->header.nextAddr += temp.data.length;
    }
    /* Make header data length */
    mem->header.dataLength = temp.data.length;
    /* Make header type */
    mem->header.type = MEMORY_HEADER_TYPE;
    /* Make data address */
    mem->data.addr = temp.header.nextAddr + _header2data_offset_length;
    /* Make CRC */
    mem->header.crc32 = calculator_crc32(mem);

#if(1)
    FWL_TAG_INFO("[setHeader] addr 0x%x", mem->header.addr);
    FWL_TAG_INFO("[setHeader] crc32 0x%x", mem->header.crc32);
    FWL_TAG_INFO("[setHeader] header.nextAddr 0x%x", mem->header.nextAddr);
    FWL_TAG_INFO("[setHeader] header.prevAddr 0x%x", mem->header.prevAddr);
    FWL_TAG_INFO("[setHeader] dataLength %u", mem->header.dataLength);
#endif

    uint16_t length = _header2data_offset_length;
    if (!writeOut(mem->header.addr, (uint8_t *)mem->header.crc32, &length))
    {
        FWL_TAG_INFO("[setHeader] Write Data failed!");
        return false;
    }

    if (length != _header2data_offset_length)
    {
        FWL_TAG_INFO("[setHeader] Write length failed!");
        return false;
    }

    return true;
}

/**
 * @brief Get data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::getData(memory_cxt_t* mem)
{
    if (mem->data.pBuffer == NULL)
    {
        FWL_TAG_INFO("[getData] Data buffer NULL");
        return false;
    }

    /* Update data location address */
    mem->data.addr = mem->header.addr + _header2data_offset_length;
    /* Get data */
    mem->data.length = mem->header.dataLength;
    if (!_pCallbacks->onRead(mem->data.addr, mem->data.pBuffer, &mem->data.length))
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
    if(calculator_crc32(mem) != mem->header.crc32)
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

/**
 * @brief set data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::setData(memory_cxt_t* mem)
{
    if (!setHeader(mem))
    {
        FWL_TAG_INFO("[setData] header failed!");
        return false;
    }

    /* write data */
    mem->data.length = mem->header.dataLength;
    if (!writeOut(mem->data.addr, mem->data.pBuffer, &mem->data.length))
    {
        FWL_TAG_INFO("[setData] Write Data failed!");
        return false;
    }

    if (mem->data.length != mem->header.dataLength)
    {
        FWL_TAG_INFO("[setData] Write length failed!");
        return false;
    }

    /* Erase CRC next header */
#if(0) /* Not need, because The flash must be erase before write anything */
    if ((mem->header.nextAddr + 4) <= (_start_addr + _memory_size))
    {
        uint8_t buff[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        uint16_t length = 4;
        
        if (!writeOut(mem->header.nextAddr, buff, &length))
        {
            FWL_TAG_INFO("[setData] Erase CRC failed!");
            return false;
        }
        if (length != 4)
        {
            FWL_TAG_INFO("[setData] Erase CRC length failed!");
            return false;
        }
    }
#endif

#if(1)
    FWL_TAG_INFO("[setData] addr 0x%x", mem->data.addr);
    FWL_TAG_INFO("[setData] length %u", mem->data.length);
    FWL_TAG_INFO("[setData] succeed");
#endif

    return true;
}

/**
 * @brief writeout data.
 * @param [in] addr The address write into flash.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::writeOut(uint32_t addr, uint8_t* buff, uint16_t* length)
{
    if (eraseData(addr, *length))
    {
        FWL_TAG_INFO("[writeOut] erase failed!");
        return false;
    }

    if (!_pCallbacks->onWrite(addr, buff, length))
    {
        FWL_TAG_INFO("[writeOut] write failed!");
        return false;
    }

    return true;
}

/**
 * @brief erase data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::eraseData(uint32_t addr, uint16_t length)
{
    uint32_t offset_addr, remain_size, erase_addr;
    bool status_isOK = true;

    offset_addr = addr % _page_size;
    remain_size = _page_size - offset_addr;
    erase_addr = addr + remain_size;

    /* Write data with write_length 
        uint32_t write_length;
        if (length >= remain_size)
        {
            write_length = remain_size;
        }
        else
        {
            write_length = length;
        }
        _pCallbacks->onWrite(addr, buff, write_length);
    */

    while (length >= remain_size)
    {
        /* Assert memory size */
        if ((erase_addr + _page_size) > (_start_addr + _memory_size))
        {
            status_isOK = false;
            break;
        }

        /* Erase next page */
        if(!_pCallbacks->onErase(erase_addr, _page_size))
        {
            status_isOK = false;
            break;
        }

        /* Remain Length */
        length -= remain_size;
        /* Write data with write_length
            if (length >= _page_size)
            {
                write_length = _page_size;
            }
            else
            {
                write_length = length;
            }

            if(write_length > 0)
            {
                _pCallbacks->onWrite(erase_addr, buff, write_length);
            }
        */

        /* new remain size is equal page size */
        remain_size = _page_size;

        /* update address to erase next page */
        erase_addr += _page_size;
    } // while (length >= remain_size)

    return status_isOK;
}

#endif
