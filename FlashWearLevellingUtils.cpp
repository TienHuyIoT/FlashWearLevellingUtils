#include "FlashWearLevellingUtils.h"
#include "util_crc32.h"

/**
 * @brief Calculator CRC32 memory.
 */
static uint32_t fwl_header_crc32(memory_cxt_t* mem)
{
    uint32_t crc;

    CRC32_Start(0);
    /* Calculator CRC 12-byte of header*/
    CRC32_Accumulate((uint8_t *) &(mem->header.nextAddr), 12U);
    CRC32_Accumulate((uint8_t *) mem->data.pBuffer, mem->data.length);
    crc = CRC32_Get();
    
    FWL_TAG_INFO("CRC32: %08X", crc);
    return crc;
} // fwl_header_crc32

FlashWearLevellingUtils::
    FlashWearLevellingUtils(uint32_t start_addr,
                            size_t memory_size,
                            uint16_t page_erase_size,
                            uint16_t data_length) : _start_addr(start_addr),
                                                  _memory_size(memory_size),
                                                  _page_erase_size(page_erase_size),
                                                  _header2data_offset_length(16U),
                                                  _data_length(data_length)
{
    _pCallbacks = &defaultCallback;
    headerDefault();
} // FlashWearLevellingUtils

FlashWearLevellingUtils::~FlashWearLevellingUtils() {}

/**
 * Find and update current memory header
*/
bool FlashWearLevellingUtils::begin(bool formatOnFail)
{
    FWL_TAG_INFO("[begin] start_addr: %u(0x%x)", _start_addr, _start_addr);
    FWL_TAG_INFO("[begin] memory_size: %u(0x%x)", _memory_size, _memory_size);
    FWL_TAG_INFO("[begin] page_erase_size: %u(0x%x)", _page_erase_size, _page_erase_size);

    FWL_TAG_INFO("[begin] verify memory information");
    if (!verifyMemInfo())
    {
        FWL_TAG_INFO("[begin] memory info Failed!");
        return false;
    }

    FWL_TAG_INFO("[begin] findLastHeader start");
    if (!findLastHeader())
    {
        if (formatOnFail)
        {
            if (format())
            {
                FWL_TAG_INFO("[begin] the header default!");
                headerDefault();
            }
            else
            {
                FWL_TAG_INFO("[begin] format failed!");
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
bool FlashWearLevellingUtils::format()
{
    bool format = true;

    /* Allocate dynamic memory */
    uint8_t *ptr_data = new (std::nothrow) uint8_t[_page_erase_size];
    if (ptr_data != nullptr)
    {
        uint16_t length = _page_erase_size;
        FWL_TAG_INFO("[format] verify the first page erase");
        if (_pCallbacks->onRead(_start_addr, ptr_data, &length))
        {
            format = false;
            for (int i = 0; i < length; ++i)
            {
                if (ptr_data[i] != 0xFF)
                {
                    format = true;
                    break;
                }
            }
        }
        delete[] ptr_data;
    }
    
    if (format)
    {
        /* Erase first page */
        FWL_TAG_INFO("[format] erase first page");
        if (!_pCallbacks->onErase(_start_addr, _page_erase_size))
        {
            FWL_TAG_INFO("[format] erase failed!");
            return false;
        }
    }
    else
    {
        FWL_TAG_INFO("[format] No");
    }
    return true;
} // format

/**
 * Return memory information current
*/
memory_cxt_t FlashWearLevellingUtils::info()
{
    return _memory_cxt;
} // info

/** Write data to a region allocated
 * 
 *  @param buff     Buffer of data to write
 *  @param length   Size to write in bytes
 *  @return         True if read is succeed
 */
bool FlashWearLevellingUtils::write(uint8_t *buff, uint16_t *length)
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

/** Read data from a region allocated
 * 
 *  @param buff     Buffer of data to read
 *  @param length   Size to read in bytes
 *  @return         True if read is succeed
 */
bool FlashWearLevellingUtils::read(uint8_t *buff, uint16_t *length)
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
void FlashWearLevellingUtils::setCallbacks(FlashWearLevellingCallbacks *pCallbacks)
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
bool FlashWearLevellingUtils::findLastHeader()
{
    memory_cxt_t mem_cxt = {0};
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
        headerDefault();
    }
    return false;
} // findLastHeader

void FlashWearLevellingUtils::headerDefault(void)
{
    _memory_cxt.header.addr = _start_addr;
    _memory_cxt.header.nextAddr = _start_addr;
    _memory_cxt.header.prevAddr = _start_addr;
    _memory_cxt.header.crc32 = 0xFFFFFFFF;
    _memory_cxt.header.dataLength = 0;
    _memory_cxt.header.type = MEMORY_HEADER_TYPE;
    _memory_cxt.data.addr = _start_addr + _header2data_offset_length;
    _memory_cxt.data.length = 0;
}

/**
 * @brief Get data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
bool FlashWearLevellingUtils::loadHeader(memory_cxt_t *mem)
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

#if (1)
    FWL_TAG_INFO("[loadHeader] header.crc32 0x%X", mem->header.crc32);
    FWL_TAG_INFO("[loadHeader] header.nextAddr %u(0x%X)", mem->header.nextAddr, mem->header.nextAddr);
    FWL_TAG_INFO("[loadHeader] header.addr %u(0x%X)", mem->header.addr, mem->header.addr);
    FWL_TAG_INFO("[loadHeader] header.prevAddr %u(0x%X)", mem->header.prevAddr, mem->header.prevAddr);
    FWL_TAG_INFO("[loadHeader] header.dataLength %u", mem->header.dataLength);
#endif

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

    if ((mem->header.dataLength > MEMORY_LENGTH_MAX) || (0 == mem->header.dataLength))
    {
        FWL_TAG_INFO("[loadHeader] Header length failed!");
        return false;
    }

    if ((mem->header.nextAddr != MEMORY_HEADER_END) && (mem->header.nextAddr > (_start_addr + _memory_size)))
    {
        FWL_TAG_INFO("[loadHeader] Header nextAddr failed!");
        return false;
    }

    return true;
} // loadHeader

/**
 * @brief Get data from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 */
bool FlashWearLevellingUtils::header_isDefault(void)
{
    memory_cxt_t mem = {0};
    uint16_t length;

    /* Get header */
    mem.header.addr = _start_addr;
    length = _header2data_offset_length;
    if (!_pCallbacks->onRead(mem.header.addr, (uint8_t *)&mem.header.crc32, &length))
    {
        FWL_TAG_INFO("[header_isDefault] Read Header failed!");
        return false;
    }

    if (length != _header2data_offset_length)
    {
        FWL_TAG_INFO("[header_isDefault] Read length failed!");
        return false;
    }

    if ((0xFFFFFFFF != mem.header.crc32)
        || (_start_addr != mem.header.nextAddr)
        || (_start_addr != mem.header.prevAddr)
        || (0 != mem.header.dataLength)
        || (MEMORY_HEADER_TYPE != mem.header.type))
    {
        FWL_TAG_INFO("[header_isDefault] false!");
        return false;
    }

#if (1)
    FWL_TAG_INFO("[header_isDefault] true!");
    FWL_TAG_INFO("[header_isDefault] crc32 0x%X", mem.header.crc32);
    FWL_TAG_INFO("[header_isDefault] header.nextAddr %u(0x%X)", mem.header.nextAddr, mem.header.nextAddr);
    FWL_TAG_INFO("[header_isDefault] header.addr %u(0x%X)", mem.header.addr, mem.header.addr);
    FWL_TAG_INFO("[header_isDefault] header.prevAddr %u(0x%X)", mem.header.prevAddr, mem.header.prevAddr);
    FWL_TAG_INFO("[header_isDefault] header.dataLength %u", mem.header.dataLength);
#endif

    return true;
} // header_isDefault

/**
 * @brief set header into flash memory from header information.
 * @param [in] memory_cxt_t The structure content variable informations.
 * @Notify data buffer and data length must be available before call saveHeader()
 */
bool FlashWearLevellingUtils::saveHeader(memory_cxt_t *mem)
{
    memory_cxt_t temp = *mem;

    if (mem->data.pBuffer == NULL)
    {
        FWL_TAG_INFO("[saveHeader] Data buffer NULL");
        return false;
    }

    if ((mem->data.length > MEMORY_LENGTH_MAX) || (0 == mem->data.length))
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
        if (!_pCallbacks->onErase(_start_addr, _page_erase_size))
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
    mem->header.crc32 = fwl_header_crc32(mem);

#if (1)
    FWL_TAG_INFO("[saveHeader] header.crc32 0x%X", mem->header.crc32);
    FWL_TAG_INFO("[saveHeader] header.nextAddr %u(0x%X)", mem->header.nextAddr, mem->header.nextAddr);
    FWL_TAG_INFO("[saveHeader] header.addr %u(0x%X)", mem->header.addr, mem->header.addr);
    FWL_TAG_INFO("[saveHeader] header.prevAddr %u(0x%X)", mem->header.prevAddr, mem->header.prevAddr);
    FWL_TAG_INFO("[saveHeader] header.dataLength %u", mem->header.dataLength);
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
bool FlashWearLevellingUtils::verifyHeader(memory_cxt_t *mem)
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

    if ((mem->header.dataLength > MEMORY_LENGTH_MAX) || (0 == mem->header.dataLength))
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
bool FlashWearLevellingUtils::loadData(memory_cxt_t *mem)
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

    if ((mem->data.length > MEMORY_LENGTH_MAX) || (0 == mem->data.length))
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
    FWL_TAG_INFO("[loadData] fwl_header_crc32");
    if (fwl_header_crc32(mem) != mem->header.crc32)
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
bool FlashWearLevellingUtils::saveData(memory_cxt_t *mem)
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
bool FlashWearLevellingUtils::submitData(uint32_t addr, uint8_t *buff, uint16_t *length)
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
bool FlashWearLevellingUtils::eraseData(uint32_t addr, uint16_t length)
{
    uint32_t offset_addr, remain_size, erase_addr, page_erase_cnt;
    bool status_isOK = true;

    offset_addr = addr % _page_erase_size;
    remain_size = _page_erase_size - offset_addr;
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
        if ((erase_addr + _page_erase_size) > (_start_addr + _memory_size))
        {
            FWL_TAG_INFO("[eraseData] addr failed!");
            status_isOK = false;
            break;
        }

        /* Erase next page */
        if (!_pCallbacks->onErase(erase_addr, _page_erase_size))
        {
            FWL_TAG_INFO("[eraseData] erase failed!");
            status_isOK = false;
            break;
        }

        ++page_erase_cnt;

        /* Remain Length */
        length -= remain_size;
        /* Write data with write_length
            if (length >= _page_erase_size)
            {
                write_length = _page_erase_size;
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
        remain_size = _page_erase_size;

        /* update address to erase next page */
        erase_addr += _page_erase_size;

        FWL_TAG_INFO("[eraseData] remain length: %u", length);
        FWL_TAG_INFO("[eraseData] remain_size: %u", remain_size);
    } // while (length >= remain_size)

    if (page_erase_cnt > 0)
    {
        FWL_TAG_INFO("[eraseData] page number is erased: %u", page_erase_cnt);
    }
    else
    {
        FWL_TAG_INFO("[eraseData] No");
    }

    return status_isOK;
} // eraseData

/**
 * verify memory information
*/
bool FlashWearLevellingUtils::verifyMemInfo()
{
    if ((_start_addr % _page_erase_size))
    {
        FWL_TAG_INFO("[verifyMemInfo][error] _start_addr must be multiples _page_erase_size");
        return false;
    }

    if ((0 == _memory_size) || (_memory_size % _page_erase_size))
    {
        FWL_TAG_INFO("[verifyMemInfo][error] _memory_size must be multiples _page_erase_size");
        return false;
    }

    if ((_data_length + _header2data_offset_length) > _memory_size)
    {
        FWL_TAG_INFO("[verifyMemInfo][error] _memory_size is not enough, min = %u", _data_length + _header2data_offset_length);
        return false;
    }

    return true;
} // verifyMemInfo

/**
 * The callback handler flash memory
*/
FlashWearLevellingCallbacks::~FlashWearLevellingCallbacks() {}

/**
 * @brief Callback function to support a read request.
 * @param [in] addr The address read from flash.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
bool FlashWearLevellingCallbacks::onRead(uint32_t addr, uint8_t *buff, uint16_t *length)
{
    FWL_TAG_INFO(">> onRead: default << onRead");
    return true;
} // onRead

/**
 * @brief Callback function to support a write request.
 * @param [in] addr The address write into flash.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
bool FlashWearLevellingCallbacks::onWrite(uint32_t addr, uint8_t *buff, uint16_t *length)
{
    FWL_TAG_INFO(">> onWrite: default << onWrite");
    return true;
} // onWrite

/**
 * @brief Callback function to support a erase request.
 * @param [in] addr The address erase
 * @param [in] length The length of buffer erase
 */
bool FlashWearLevellingCallbacks::onErase(uint32_t addr, uint16_t length)
{
    FWL_TAG_INFO(">> onErase: default << onErase");
    return true;
} // onErase

/**
 * @brief Callback function to support a flash status.
 */
bool FlashWearLevellingCallbacks::onReady()
{
    FWL_TAG_INFO(">> onReady: default << onReady");
    return true;
} // onReady

/**
 * @brief Callback function to support a Status report.
 * @param [in] s Status of the process handle in memory
 */
void FlashWearLevellingCallbacks::onStatus(status_t s)
{
    FWL_TAG_INFO(">> onStatus: default << onStatus");
} // onStatus

FlashWearLevellingCallbacks defaultCallback; //null-object-pattern
