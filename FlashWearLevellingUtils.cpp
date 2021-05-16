#include "FlashWearLevellingUtils.h"

#define FWL_TAG_INFO(f_, ...) printf("\r\n[FWL ] " f_, ##__VA_ARGS__)

static FlashWearLevellingCallbacks defaultCallback; //null-object-pattern

template <class varType>
FlashWearLevellingUtils<varType>::FlashWearLevellingUtils(uint32_t start_addr, size_t memory_size, uint16_t page_size) :
_start_addr(start_addr),
_memory_size(memory_size),
_page_size(page_size)
{
    _current_addr = start_addr;
    _pCallbacks = &defaultCallback;
} // FlashWearLevellingUtils

template <class varType>
FlashWearLevellingUtils<varType>::~FlashWearLevellingUtils() {}

/**
 * Find and update current memory header
*/
template <class varType>
bool FlashWearLevellingUtils<varType>::begin() {
    findLastHeader(&_header);
    return true;
} // begin

template <class varType>
bool FlashWearLevellingUtils<varType>::write(varType* data) {
    /* todo ... */
    if(_pCallbacks->onReady()) {
        uint32_t length = sizeof(varType);
        _pCallbacks->onWrite((uint8_t*)data, &length);
    }
    return true;
} // write

template <class varType>
bool FlashWearLevellingUtils<varType>::read(varType* data) {
    /* todo ... */
    if(_pCallbacks->onReady()) {
        uint32_t length = sizeof(varType);
        _pCallbacks->onRead((uint8_t*)data, &length);
    }
    return true;
} // read

template <class varType>
bool FlashWearLevellingUtils<varType>::write(uint8_t* buff, size_t length) {
    /* todo ... */
    if(_pCallbacks->onReady()) {
        _pCallbacks->onWrite(buff, &length);
    }
    return true;
} // write

template <class varType>
bool FlashWearLevellingUtils<varType>::read(uint8_t* buff, size_t length) {
    /* todo ... */
    if(_pCallbacks->onReady()) {
        _pCallbacks->onRead(buff, &length);
    }
    return true;
} // read

/**
 * @brief Set the callback handlers for this flash memory.
 * @param [in] pCallbacks An instance of a callbacks structure used to define any callbacks for the flash memory.
 */
template <class varType>
void FlashWearLevellingUtils<varType>::setCallbacks(FlashWearLevellingCallbacks* pCallbacks) {
    if (pCallbacks != nullptr) {
        _pCallbacks = pCallbacks;
    } else {
        _pCallbacks = &defaultCallback;
    }
    FWL_TAG_INFO(">> setCallbacks: 0x%x << setCallbacks", (uint32_t)_pCallbacks);
} // setCallbacks

/**
 * @brief Calculator CRC32 buffer.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
template <class varType>
uint32_t FlashWearLevellingUtils<varType>::crc32(uint8_t* buff, size_t length) {
    return 0xFFFFFFFF;
} // crc32

/**
 * @brief Calculator CRC32 buffer.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
template <class varType>
bool FlashWearLevellingUtils<varType>::findLastHeader(memoryHeader_t *header) {
    FWL_TAG_INFO(">> findLastHeader");
    return true;
} // findLastHeader

/**
 * The callback handler flash memory
*/
FlashWearLevellingCallbacks::~FlashWearLevellingCallbacks() {}

/**
 * @brief Callback function to support a read request.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
bool FlashWearLevellingCallbacks::onRead(uint8_t* buff, size_t* length) {
    FWL_TAG_INFO(">> onRead: default << onRead");
    return true;
} // onRead

/**
 * @brief Callback function to support a write request.
 * @param [in] buff The buffer data write into flash.
 * @param [in] length The length of buffer write into flash.
 */
bool FlashWearLevellingCallbacks::onWrite(uint8_t* buff, size_t* length) {
    FWL_TAG_INFO(">> onWrite: default << onWrite");
    return true;
} // onWrite

/**
 * @brief Callback function to support a erase request.
 * @param [in] page The page number need to erase.
 */
bool FlashWearLevellingCallbacks::onErase(uint32_t page) {
    FWL_TAG_INFO(">> onErase: default << onErase");
    return true;
} // onErase

/**
 * @brief Callback function to support a flash status.
 */
bool FlashWearLevellingCallbacks::onReady() {
    FWL_TAG_INFO(">> onReady: default << onReady");
    return true;
} // onReady

/**
 * @brief Callback function to support a Status report.
 * @param [in] s Status of the process handle in memory
 */
void FlashWearLevellingCallbacks::onStatus(status_t s) {
    FWL_TAG_INFO(">> onStatus: default << onStatus");
} // onStatus
