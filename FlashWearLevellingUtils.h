#ifndef __FLASH_WEAR_LEVELLING_UTILS_H
#define __FLASH_WEAR_LEVELLING_UTILS_H

#include "mbed.h"
#include <array>
#include <string>

#define MEMORY_SIZE_DEFAULT 4096U /* 4KB */
#define PAGE_SIZE_DEFAULT 256U /* 256-Byte */

#define FWL_TAG_INFO(f_, ...) //printf("\r\n[FWL ] " f_, ##__VA_ARGS__)

typedef struct __attribute__((packed, aligned(4)))
{
    struct
    {
        uint8_t *pBuffer;
        uint32_t addr;
        uint16_t length;
    } data;

    struct
    {
        uint32_t addr;
        /* I have a trick allocate memory with order as following*/
        uint32_t crc32;
        uint32_t nextAddr;
        uint32_t prevAddr;
        uint16_t dataLength;
        uint16_t type;
        /* End trick */
    } header;
} memory_cxt_t;

class FlashWearLevellingCallbacks
{
private:
    const std::array<std::string, 9> _statusStr;

public:
    typedef enum
    {
        SUCCESS_WRITE,
        SUCCESS_READ,
        ERROR_CRC_HEADER,
        ERROR_TYPE_HEADER,
        ERROR_SIZE_HEADER,
        ERROR_CRC_DATA,
        ERROR_SIZE_DATA,
        ERROR_WRITE_DATA,
        ERROR_READ_DATA
    } status_t;

    FlashWearLevellingCallbacks() : _statusStr({"SUCCESS_WRITE",
                                                "SUCCESS_READ",
                                                "ERROR_CRC_HEADER",
                                                "ERROR_TYPE_HEADER",
                                                "ERROR_SIZE_HEADER",
                                                "ERROR_CRC_DATA",
                                                "ERROR_SIZE_DATA",
                                                "ERROR_WRITE_DATA",
                                                "ERROR_READ_DATA"})
    {
    }

    virtual ~FlashWearLevellingCallbacks();
    virtual bool onRead(uint32_t addr, uint8_t *buff, uint16_t *length);
    virtual bool onWrite(uint32_t addr, uint8_t *buff, uint16_t *length);
    virtual bool onErase(uint32_t addr, uint16_t length);
    virtual bool onReady();
    virtual void onStatus(status_t s);
    std::string reportStr(status_t s)
    {
        return _statusStr[s];
    }
};

extern FlashWearLevellingCallbacks defaultCallback;

extern uint32_t calculator_crc32(memory_cxt_t *mem);

template <class varType>
class FlashWearLevellingUtils
{
    typedef enum
    {
        MEMORY_HEADER_TYPE = 0xAA55,
        MEMORY_LENGTH_MAX = 256,
        MEMORY_HEADER_END = 0xFFFFFFFF
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
    memory_cxt_t *info();

private:
    const uint16_t _data_length;
    const uint8_t _header2data_offset_length;
    varType _var;
    size_t _memory_size;
    uint32_t _start_addr;
    memory_cxt_t _memory_cxt;
    uint16_t _page_size;
    FlashWearLevellingCallbacks *_pCallbacks;
    bool findLastHeader();
    bool loadHeader(memory_cxt_t *mem);
    bool saveHeader(memory_cxt_t *mem);
    bool verifyHeader(memory_cxt_t *mem);
    bool loadData(memory_cxt_t *mem);
    bool saveData(memory_cxt_t *mem);
    bool eraseData(uint32_t addr, uint16_t length);
    bool submitData(uint32_t addr, uint8_t *buff, uint16_t *length);
    bool verifyMemInfo();
};

template <class varType>
FlashWearLevellingUtils<varType>::
    FlashWearLevellingUtils(uint32_t start_addr,
                            size_t memory_size,
                            uint16_t page_size) : _start_addr(start_addr),
                                                  _memory_size(memory_size),
                                                  _page_size(page_size),
                                                  _header2data_offset_length(16U),
                                                  _data_length(sizeof(varType))
{
    _pCallbacks = &defaultCallback;
    memset(&_memory_cxt, 0, sizeof(memory_cxt_t));
    _memory_cxt.header.addr = start_addr;
    _memory_cxt.header.prevAddr = start_addr;
    _memory_cxt.header.nextAddr = MEMORY_HEADER_END;
    _memory_cxt.header.crc32 = 0xFFFFFFFF;
} // FlashWearLevellingUtils

template <class varType>
FlashWearLevellingUtils<varType>::~FlashWearLevellingUtils() {}

/**
 * Find and update current memory header
*/
template <class varType>
bool FlashWearLevellingUtils<varType>::begin(bool formatOnFail)
{
    FWL_TAG_INFO("[begin] start_addr: %u(0x%x)", _start_addr, _start_addr);
    FWL_TAG_INFO("[begin] memory_size: %u(0x%x)", _memory_size, _memory_size);
    FWL_TAG_INFO("[begin] page_size: %u(0x%x)", _page_size, _page_size);

    FWL_TAG_INFO("[begin] verify memory information");
    if (!verifyMemInfo())
    {
        FWL_TAG_INFO("[begin] memory info Failed!");
        return false;
    }

    memset(&_memory_cxt, 0, sizeof(memory_cxt_t));
    _memory_cxt.header.addr = _start_addr;
    _memory_cxt.header.prevAddr = _start_addr;
    _memory_cxt.header.nextAddr = MEMORY_HEADER_END;
    _memory_cxt.header.crc32 = 0xFFFFFFFF;

    FWL_TAG_INFO("[begin] findLastHeader start");
    if (!findLastHeader())
    {
        if (formatOnFail)
        {
            /* Erase first page */
            FWL_TAG_INFO("[begin] erase first page");
            if (!_pCallbacks->onErase(_start_addr, _page_size))
            {
                FWL_TAG_INFO("[begin] erase failed!");
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    return true;
} // begin

/**
 * Format memory type as factory
*/
template <class varType>
bool FlashWearLevellingUtils<varType>::format()
{
    /* Erase first page */
    FWL_TAG_INFO("[format] erase first page");
    if (!_pCallbacks->onErase(_start_addr, _page_size))
    {
        FWL_TAG_INFO("[format] erase failed!");
        return false;
    }

    FWL_TAG_INFO("[format] Init header default");
    /* Don't have any header, this mean have not data */
    _memory_cxt.header.addr = _start_addr;
    _memory_cxt.header.nextAddr = _start_addr;
    _memory_cxt.header.prevAddr = _start_addr;
    _memory_cxt.header.crc32 = 0xFFFFFFFF;
    _memory_cxt.header.dataLength = 0;
    _memory_cxt.header.type = MEMORY_HEADER_TYPE;
    _memory_cxt.data.addr = _start_addr + _header2data_offset_length;
    _memory_cxt.data.length = 0;
    return true;
} // format

/**
 * Format memory type as factory
*/
template <class varType>
memory_cxt_t *FlashWearLevellingUtils<varType>::info()
{
    return &_memory_cxt;
} // format

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
    if (!verifyMemInfo())
    {
        FWL_TAG_INFO("[write] memory info Failed!");
        return false;
    }

    memory_cxt_t w_memory = _memory_cxt;
    w_memory.data.pBuffer = buff;
    w_memory.data.length = *length;
    if (saveData(&w_memory))
    {
        _memory_cxt = w_memory;
        return true;
    }
    return false;
} // write

template <class varType>
bool FlashWearLevellingUtils<varType>::read(uint8_t *buff, uint16_t *length)
{
    if (!verifyMemInfo())
    {
        FWL_TAG_INFO("[read] memory info Failed!");
        return false;
    }

    if ((*length) < _memory_cxt.header.dataLength)
    {
        FWL_TAG_INFO("[read] length buffer is not enough!");
        return false;
    }

    FWL_TAG_INFO("[read] load data");
    /* Read data */
    _memory_cxt.data.pBuffer = buff;
    /* The data length must be equal header data length */
    _memory_cxt.data.length = _memory_cxt.header.dataLength;
    if (!loadData(&_memory_cxt))
    {
        FWL_TAG_INFO("[read] Data failed!");
        return false;
    }

    FWL_TAG_INFO("[read] data length: %u", _memory_cxt.data.length);
    *length = _memory_cxt.data.length;

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
    do
    {
        /* End */
        if (MEMORY_HEADER_END == mem_cxt.header.addr)
        {
            FWL_TAG_INFO("[findLastHeader] Header Addr End!");
            break;
        }

        /* Get header */
        mem_cxt.header.addr = mem_cxt.header.nextAddr;
        if (!loadHeader(&mem_cxt))
        {
            FWL_TAG_INFO("[findLastHeader] Read Header failed!");
            break;
        }

        /* Read data with the purpose is verify data content in flash memory 
            have to succeed before next header */
        mem_cxt.data.pBuffer = ptr_data;
        mem_cxt.data.length = mem_cxt.header.dataLength;
        mem_cxt.data.addr = mem_cxt.header.addr + _header2data_offset_length;
        if (!loadData(&mem_cxt))
        {
            FWL_TAG_INFO("[findLastHeader] Data failed!");
            break;
        }

        /* Update counter found a header */
        find_cnt++;
        FWL_TAG_INFO("Found %u", find_cnt);

        /* Save last mem_cxt */
        _memory_cxt = mem_cxt;
    } while (1);

    delete[] ptr_data; /* free memory */
    FWL_TAG_INFO("[findLastHeader] Free %u byte RAM at 0x%x", MEMORY_LENGTH_MAX, (uint32_t)ptr_data);

    if (find_cnt > 0)
    {
        FWL_TAG_INFO("[findLastHeader] Final counter: %u", find_cnt);
#if (1)
        FWL_TAG_INFO("[findLastHeader] Addr %u(0x%X)", _memory_cxt.header.addr, _memory_cxt.header.addr);
        FWL_TAG_INFO("[findLastHeader] crc32 0x%x", _memory_cxt.header.crc32);
        FWL_TAG_INFO("[findLastHeader] next Addr %u(0x%X)", _memory_cxt.header.nextAddr, _memory_cxt.header.nextAddr);
        FWL_TAG_INFO("[findLastHeader] prev Addr %u(0x%X)", _memory_cxt.header.prevAddr, _memory_cxt.header.prevAddr);
        FWL_TAG_INFO("[findLastHeader] data length %u", _memory_cxt.header.dataLength);
#endif
        return true;
    }
    else
    {
        FWL_TAG_INFO("[findLastHeader] Not Found header");
        FWL_TAG_INFO("[findLastHeader] Init header default");
        /* Don't have any header, this mean have not data */
        _memory_cxt.header.addr = _start_addr;
        _memory_cxt.header.nextAddr = _start_addr;
        _memory_cxt.header.prevAddr = _start_addr;
        _memory_cxt.header.crc32 = 0xFFFFFFFF;
        _memory_cxt.header.dataLength = 0;
        _memory_cxt.header.type = MEMORY_HEADER_TYPE;
        _memory_cxt.data.addr = _start_addr + _header2data_offset_length;
        _memory_cxt.data.length = 0;
    }
    return false;
} // findLastHeader

/**
 * @brief Get data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::loadHeader(memory_cxt_t *mem)
{
    uint16_t length;

    if (MEMORY_HEADER_END == mem->header.addr)
    {
        FWL_TAG_INFO("[loadHeader] Header Addr End!");
        return false;
    }
    /* Get header */
    length = _header2data_offset_length;
    if (!_pCallbacks->onRead(mem->header.addr, (uint8_t *)&mem->header.crc32, &length))
    {
        FWL_TAG_INFO("[loadHeader] Read Header failed!");
        return false;
    }

    if (length != _header2data_offset_length)
    {
        FWL_TAG_INFO("[loadHeader] Read length failed!");
        return false;
    }

    if (0xFFFFFFFF == mem->header.crc32)
    {
        FWL_TAG_INFO("[loadHeader] Header CRC32 failed!");
        return false;
    }

    if (mem->header.type != MEMORY_HEADER_TYPE)
    {
        FWL_TAG_INFO("[loadHeader] Header type failed!");
        return false;
    }

    if (mem->header.dataLength > MEMORY_LENGTH_MAX)
    {
        FWL_TAG_INFO("[loadHeader] Header length failed!");
        return false;
    }

    if ((mem->header.nextAddr != MEMORY_HEADER_END) && (mem->header.nextAddr > (_start_addr + _memory_size)))
    {
        FWL_TAG_INFO("[loadHeader] Header nextAddr failed!");
        return false;
    }

#if (1)
    FWL_TAG_INFO("[loadHeader] crc32 0x%X", mem->header.crc32);
    FWL_TAG_INFO("[loadHeader] header.nextAddr %u(0x%X)", mem->header.nextAddr, mem->header.nextAddr);
    FWL_TAG_INFO("[saveHeader] header.addr %u(0x%X)", mem->header.addr, mem->header.addr);
    FWL_TAG_INFO("[loadHeader] header.prevAddr %u(0x%X)", mem->header.prevAddr, mem->header.prevAddr);
    FWL_TAG_INFO("[loadHeader] dataLength %u", mem->header.dataLength);
#endif

    return true;
} // loadHeader

/**
 * @brief set header into flash memory from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 * @Notify data buffer and data length must be available before call saveHeader()
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::saveHeader(memory_cxt_t *mem)
{
    memory_cxt_t temp = *mem;

    if (mem->data.pBuffer == NULL)
    {
        FWL_TAG_INFO("[saveHeader] Data buffer NULL");
        return false;
    }

    if (mem->data.length > MEMORY_LENGTH_MAX)
    {
        FWL_TAG_INFO("[saveHeader] Data length failed!");
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
    if (mem->header.nextAddr > (_start_addr + _memory_size))
    {
        FWL_TAG_INFO("[saveHeader] Reset address");
        /* Reset address header base equal _start_addr */
        mem->header.addr = _start_addr;
        /* Make header next address again */
        mem->header.nextAddr = _start_addr;
        mem->header.nextAddr += _header2data_offset_length;
        mem->header.nextAddr += temp.data.length;

        /* Reset temp header address */
        temp.header.nextAddr = _start_addr;

        /* Erase first page */
        FWL_TAG_INFO("[saveHeader] erase first page");
        if (!_pCallbacks->onErase(_start_addr, _page_size))
        {
            FWL_TAG_INFO("[saveHeader] erase failed!");
            return false;
        }
    }
    /* Make header data length */
    mem->header.dataLength = temp.data.length;
    /* Make header type */
    mem->header.type = MEMORY_HEADER_TYPE;
    /* Make data address */
    mem->data.addr = temp.header.nextAddr + _header2data_offset_length;
    /* Make CRC */
    mem->header.crc32 = calculator_crc32(mem);

#if (1)
    FWL_TAG_INFO("[saveHeader] crc32 0x%X", mem->header.crc32);
    FWL_TAG_INFO("[saveHeader] header.nextAddr %u(0x%X)", mem->header.nextAddr, mem->header.nextAddr);
    FWL_TAG_INFO("[saveHeader] header.addr %u(0x%X)", mem->header.addr, mem->header.addr);
    FWL_TAG_INFO("[saveHeader] header.prevAddr %u(0x%X)", mem->header.prevAddr, mem->header.prevAddr);
    FWL_TAG_INFO("[saveHeader] dataLength %u", mem->header.dataLength);
#endif

    uint16_t length = _header2data_offset_length;
    if (!submitData(mem->header.addr, (uint8_t *)&mem->header.crc32, &length))
    {
        FWL_TAG_INFO("[saveHeader] Write Data failed!");
        return false;
    }

    if (length != _header2data_offset_length)
    {
        FWL_TAG_INFO("[saveHeader] Write length failed!");
        return false;
    }

    return true;
} // saveHeader

/**
 * @brief verify header header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::verifyHeader(memory_cxt_t *mem)
{
    if (0xFFFFFFFF == mem->header.crc32)
    {
        FWL_TAG_INFO("[verifyHeader] Header CRC32 failed!");
        _pCallbacks->onStatus(FlashWearLevellingCallbacks::status_t::ERROR_CRC_HEADER);
        return false;
    }

    if (mem->header.type != MEMORY_HEADER_TYPE)
    {
        FWL_TAG_INFO("[verifyHeader] Header type failed!");
        _pCallbacks->onStatus(FlashWearLevellingCallbacks::status_t::ERROR_TYPE_HEADER);
        return false;
    }

    if (mem->header.dataLength > MEMORY_LENGTH_MAX)
    {
        FWL_TAG_INFO("[verifyHeader] Header length failed!");
        _pCallbacks->onStatus(FlashWearLevellingCallbacks::status_t::ERROR_SIZE_HEADER);
        return false;
    }

    if ((mem->header.nextAddr != MEMORY_HEADER_END) && (mem->header.nextAddr > (_start_addr + _memory_size)))
    {
        FWL_TAG_INFO("[verifyHeader] Header nextAddr failed!");
        return false;
    }

    return true;
} // verifyHeader

/**
 * @brief Get data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::loadData(memory_cxt_t *mem)
{
    FWL_TAG_INFO("[loadData] verifyHeader");
    if (!verifyHeader(mem))
    {
        FWL_TAG_INFO("[loadData] header failed!");
        return false;
    }

    if (mem->data.pBuffer == NULL)
    {
        FWL_TAG_INFO("[loadData] Data buffer NULL");
        return false;
    }

    if (mem->data.length > MEMORY_LENGTH_MAX)
    {
        FWL_TAG_INFO("[loadData] Data length failed!");
        _pCallbacks->onStatus(FlashWearLevellingCallbacks::status_t::ERROR_SIZE_DATA);
        return false;
    }

    /* Get data */
    if (!_pCallbacks->onRead(mem->data.addr, mem->data.pBuffer, &mem->data.length))
    {
        FWL_TAG_INFO("[loadData] Read Data failed!");
        return false;
    }

    if (mem->data.length != mem->header.dataLength)
    {
        FWL_TAG_INFO("[loadData] Read length failed!");
        _pCallbacks->onStatus(FlashWearLevellingCallbacks::status_t::ERROR_READ_DATA);
        return false;
    }

    /* Checksum */
    FWL_TAG_INFO("[loadData] calculator_crc32");
    if (calculator_crc32(mem) != mem->header.crc32)
    {
        FWL_TAG_INFO("[loadData] CRC32 header failed!");
        _pCallbacks->onStatus(FlashWearLevellingCallbacks::status_t::ERROR_CRC_DATA);
        return false;
    }

#if (1)
    FWL_TAG_INFO("[loadData] addr %u(0x%x)", mem->data.addr, mem->data.addr);
    FWL_TAG_INFO("[loadData] length %u", mem->data.length);
    FWL_TAG_INFO("[loadData] CRC32 succeed!");
#endif

    return true;
} // loadData

/**
 * @brief set data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::saveData(memory_cxt_t *mem)
{
    FWL_TAG_INFO("[saveData] save header");
    if (!saveHeader(mem))
    {
        FWL_TAG_INFO("[saveData] header failed!");
        return false;
    }

    /* write data */
    FWL_TAG_INFO("[saveData] save data");
    if (!submitData(mem->data.addr, mem->data.pBuffer, &mem->data.length))
    {
        FWL_TAG_INFO("[saveData] Write Data failed!");
        return false;
    }

    if (mem->data.length != mem->header.dataLength)
    {
        FWL_TAG_INFO("[saveData] Write length failed!");
        _pCallbacks->onStatus(FlashWearLevellingCallbacks::status_t::ERROR_WRITE_DATA);
        return false;
    }

    /* Erase CRC next header */
#if (0) /* Not need, because The flash must be erase before write anything */
    if ((mem->header.nextAddr + 4) <= (_start_addr + _memory_size))
    {
        uint8_t buff[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        uint16_t length = 4;

        if (!submitData(mem->header.nextAddr, buff, &length))
        {
            FWL_TAG_INFO("[saveData] Erase CRC failed!");
            return false;
        }
        if (length != 4)
        {
            FWL_TAG_INFO("[saveData] Erase CRC length failed!");
            return false;
        }
    }
#endif

#if (1)
    FWL_TAG_INFO("[saveData] addr %u(0x%x)", mem->data.addr, mem->data.addr);
    FWL_TAG_INFO("[saveData] length %u", mem->data.length);
    FWL_TAG_INFO("[saveData] succeed");
#endif

    return true;
} // saveData

/**
 * @brief writeout data.
 * @param [in] addr The address write into flash.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::submitData(uint32_t addr, uint8_t *buff, uint16_t *length)
{
    if (!eraseData(addr, *length))
    {
        FWL_TAG_INFO("[submitData] erase failed!");
        return false;
    }

    if (!_pCallbacks->onWrite(addr, buff, length))
    {
        FWL_TAG_INFO("[submitData] write failed!");
        return false;
    }

    return true;
} // submitData

/**
 * @brief erase data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::eraseData(uint32_t addr, uint16_t length)
{
    uint32_t offset_addr, remain_size, erase_addr, page_erase_cnt;
    bool status_isOK = true;

    offset_addr = addr % _page_size;
    remain_size = _page_size - offset_addr;
    erase_addr = addr + remain_size;

    FWL_TAG_INFO("[eraseData] addr in: %u(0x%x)", addr, addr);
    FWL_TAG_INFO("[eraseData] length in: %u", length);
    FWL_TAG_INFO("[eraseData] remain_size: %u", remain_size);

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
    page_erase_cnt = 0;
    while (length >= remain_size)
    {
        FWL_TAG_INFO("[eraseData] erase_addr: %u", erase_addr);

        if (erase_addr == (_start_addr + _memory_size) && (length == remain_size))
        {
            FWL_TAG_INFO("[eraseData] last page");
            break;
        }

        /* Assert memory size */
        if ((erase_addr + _page_size) > (_start_addr + _memory_size))
        {
            FWL_TAG_INFO("[eraseData] addr failed!");
            status_isOK = false;
            break;
        }

        /* Erase next page */
        if (!_pCallbacks->onErase(erase_addr, _page_size))
        {
            FWL_TAG_INFO("[eraseData] erase failed!");
            status_isOK = false;
            break;
        }

        ++page_erase_cnt;

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

        FWL_TAG_INFO("[eraseData] remain length: %u", length);
        FWL_TAG_INFO("[eraseData] remain_size: %u", remain_size);
    } // while (length >= remain_size)

    FWL_TAG_INFO("[eraseData] page number is erased: %u", page_erase_cnt);

    return status_isOK;
} // eraseData

/**
 * verify memory information
*/
template <class varType>
bool FlashWearLevellingUtils<varType>::verifyMemInfo()
{
    if ((_start_addr % _page_size))
    {
        FWL_TAG_INFO("[verifyMemInfo][error] _start_addr must be multiples _page_size");
        return false;
    }

    if ((0 == _memory_size) || (_memory_size % _page_size))
    {
        FWL_TAG_INFO("[verifyMemInfo][error] _memory_size must be multiples _page_size");
        return false;
    }

    if ((_data_length + _header2data_offset_length) > _memory_size)
    {
        FWL_TAG_INFO("[verifyMemInfo][error] _memory_size is not enough, min = %u", _data_length + _header2data_offset_length);
        return false;
    }

    return true;
} // verifyMemInfo

#endif
