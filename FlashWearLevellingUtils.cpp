#include "FlashWearLevellingUtils.h"
#include "util_crc32.h"

/**
 * @brief Calculator CRC32 buffer.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
uint32_t calculator_crc32(memory_cxt_t* mem)
{
    uint32_t crc;

    CRC32_Start(0);
    /* Calculator CRC 12-byte of header*/
    CRC32_Accumulate((uint8_t *) &(mem->header.nextAddr), 12U);
    CRC32_Accumulate((uint8_t *) mem->data.pBuffer, mem->data.length);
    crc = CRC32_Get();
    
    FWL_TAG_INFO("CRC32: %08X", crc);
    return crc;
} // calculator_crc32

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
