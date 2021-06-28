/** @file FlashWearLevellingUtils.h
 *  @brief utility support saving a variable or any type data structure 
 *         into a space NAND/NOR memory
 *
 *  @author tienhuyiot
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver    Who                      Date             Changes
 * -----  --------------------     ----------       --------------------
 * 1.0    tienhuyiot@gmail.com     Jun 06, 2021     Initialize
 *
 *
 *</pre>
 */

#ifndef __FLASH_WEAR_LEVELLING_UTILS_H
#define __FLASH_WEAR_LEVELLING_UTILS_H

#include "mbed.h"
#include <array>
#include <string>
#include "console_dbg.h"

#define MEMORY_SIZE_DEFAULT 4096U /* 4KB */
#define PAGE_ERASE_SIZE_DEFAULT 4096U /* 4096-Byte */

#define FWL_TAG_INFO(...) //CONSOLE_TAG_LOGI("[FWL]", __VA_ARGS__)
#define FWL_INFO(...) //CONSOLE_LOGI(__VA_ARGS__)

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

class FlashWearLevellingUtils
{
    typedef enum
    {
        MEMORY_HEADER_TYPE = 0xAA55,
        MEMORY_LENGTH_MAX = 256,
        MEMORY_HEADER_END = 0xFFFFFFFF
    } memoryType_t;

public:
    FlashWearLevellingUtils(uint32_t start_addr = 0, 
                            size_t memory_size = MEMORY_SIZE_DEFAULT, 
                            uint16_t page_erase_size = PAGE_ERASE_SIZE_DEFAULT,
                            uint16_t data_length = 1);
    ~FlashWearLevellingUtils();
    void setCallbacks(FlashWearLevellingCallbacks *pCallbacks);
    bool begin(bool formatOnFail = false);
    bool format();
    bool write(uint8_t *buff, uint16_t *length);
    bool read(uint8_t *buff, uint16_t *length);
    memory_cxt_t info();

    template <typename varType>
    bool write(varType *data)
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

    template <typename varType>
    bool read(varType *data)
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

private:
    const uint16_t _data_length;
    const uint8_t _header2data_offset_length;
    size_t _memory_size;
    uint32_t _start_addr;
    memory_cxt_t _memory_cxt;
    uint16_t _page_erase_size;
    FlashWearLevellingCallbacks *_pCallbacks;
    bool findLastHeader();
    bool header_isDefault(void);
    void headerDefault(void);
    bool loadHeader(memory_cxt_t *mem);
    bool saveHeader(memory_cxt_t *mem);
    bool verifyHeader(memory_cxt_t *mem);
    bool loadData(memory_cxt_t *mem);
    bool saveData(memory_cxt_t *mem);
    bool eraseData(uint32_t addr, uint16_t length);
    bool submitData(uint32_t addr, uint8_t *buff, uint16_t *length);
    bool verifyMemInfo();
};

#endif
