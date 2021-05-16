#ifndef __FLASH_WEAR_LEVELLING_UTILS_H
#define __FLASH_WEAR_LEVELLING_UTILS_H

#include "mbed.h"

#define MEMORY_SIZE_DEFAULT 4096U /* 4KB */
#define PAGE_SIZE_DEFAULT 256U /* 256-Byte */

class FlashWearLevellingCallbacks;

template <class varType>
class FlashWearLevellingUtils
{
public:
    FlashWearLevellingUtils(uint32_t start_addr = 0, size_t memory_size = MEMORY_SIZE_DEFAULT, uint16_t page_size = PAGE_SIZE_DEFAULT);
    ~FlashWearLevellingUtils();
	void setCallbacks(FlashWearLevellingCallbacks* pCallbacks);
	bool begin();
	bool write(varType* data);
	bool read(varType* data);
	bool write(uint8_t* buff, size_t length);
	bool read(uint8_t* buff, size_t length);

private:
	typedef enum {
		MEMORY_HEADER_TYPE = 0xAA55
	} memoryType_t;

	typedef struct {
		uint32_t crc32;
		uint32_t next_addr;
		uint32_t prev_addr;
		uint16_t length;
		uint16_t type;
	} memoryHeader_t;

    size_t _memory_size;
    uint16_t _page_size;
    uint32_t _start_addr;
	uint32_t _current_addr;
	memoryHeader_t _header;
	FlashWearLevellingCallbacks* _pCallbacks;
	uint32_t crc32(uint8_t* buff, size_t length);
	bool findLastHeader(memoryHeader_t *header);
};

class FlashWearLevellingCallbacks {
public:
	typedef enum {
		SUCCESS_WRITE,
		SUCCESS_READ,
		ERROR_CRC,
		ERROR_TYPE,
		ERROR_SIZE
	} status_t;

	virtual ~FlashWearLevellingCallbacks();
	virtual bool onRead(uint8_t* buff, size_t* length);
	virtual bool onWrite(uint8_t* buff, size_t* length);
	virtual bool onErase(uint32_t page);
	virtual bool onReady();
	virtual void onStatus(status_t s);
};

#endif
