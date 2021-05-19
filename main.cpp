/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "FlashWearLevellingUtils.h"

#define MAIN_TAG_INFO(f_, ...) //printf("\r\n[MAIN] " f_, ##__VA_ARGS__)
#define MAIN_TAG_DBG(f_, ...)   do{\
                                    printf("\r\n[MAIN] ");\
                                    printf(f_, ##__VA_ARGS__);\
                                    printf("\r\n");\
                                }while(0)

typedef struct {
    uint32_t a;
} flash_data_t;

/* Declare object flash data type handler */
FlashWearLevellingUtils<flash_data_t> CVChasingLog(0, 256, 64);

class v_memory {
private:
    uint8_t* pbuff;
    uint16_t _v_num;
public:
    v_memory(uint16_t v_num) : _v_num(v_num) {
        /* Allocate dynamic memory */
        pbuff = new (std::nothrow) uint8_t[_v_num];
    }

    ~v_memory()
    {
        delete[] pbuff; /* free memory */
    }

    bool write(uint32_t addr, uint8_t *buff, uint16_t *length)
    {
        if (addr + (*length) > _v_num)
        {
            return false;
        }

        memcpy(pbuff + addr, buff, *length);
        return true;
    }

    bool read(uint32_t addr, uint8_t *buff, uint16_t *length)
    {
        memcpy(buff, pbuff + addr, *length);
        return true;
    }

    bool erase(uint32_t addr, uint16_t length)
    {
        if (addr + length > _v_num)
        {
            return false;
        }

        memset(pbuff + addr, 0, length);
        return true;
    }
};

v_memory v_testAPI(1024);

/* Register callback handler external flash memory */
class FlashHandler: public FlashWearLevellingCallbacks {
    bool onRead(uint32_t addr, uint8_t *buff, uint16_t *length) {
        MAIN_TAG_INFO("[FlashHandler] onRead data [addr][length]: [%u(0x%x)][%u]", addr, addr, *length);
        return v_testAPI.read(addr, buff, length);
    }

    bool onWrite(uint32_t addr, uint8_t *buff, uint16_t *length) {
        MAIN_TAG_INFO("[FlashHandler] onWrite data [addr][length]: [%u(0x%x)][%u]", addr, addr, *length);
        return v_testAPI.write(addr, buff, length);
    }

    bool onErase(uint32_t addr, uint16_t length) {
        MAIN_TAG_INFO("flash onErase [addr][length]: [%u(0x%x)][%u]", addr, addr, length);
        return v_testAPI.erase(addr, length);
    }

    bool onReady() {
        MAIN_TAG_INFO("[FlashHandler] onReady");
        return true;
    }

    void onStatus(status_t s) {
        MAIN_TAG_DBG("[FlashHandler] onStatus [%u][%s]", (uint8_t)s, reportStr(s).c_str());
    }
};

/** Testcase
 * 1. write and read
 * 2. memory is full and retart 
*/
void fwl_testCase()
{
    flash_data_t w_data;
    flash_data_t r_data;
    w_data.a = 100;
    for (int i = 0; i < 1270 /* RANDOM */; ++i)
    {
        w_data.a++;
        if (!CVChasingLog.write(&w_data))
        {
            MAIN_TAG_DBG("[CVChasingLog] read data failed!");
            break;
        }
    }

    if (CVChasingLog.read(&r_data))
    {
        MAIN_TAG_DBG("[CVChasingLog] r_data.a = %u", r_data.a);

        memory_cxt_t* info = CVChasingLog.info();
        MAIN_TAG_DBG("[CVChasingLog] Addr: %u(0x%X)", info->header.addr, info->header.addr);
        MAIN_TAG_DBG("[CVChasingLog] crc32: 0x%X", info->header.crc32);
        MAIN_TAG_DBG("[CVChasingLog] next Addr: %u(0x%X)", info->header.nextAddr, info->header.nextAddr);
        MAIN_TAG_DBG("[CVChasingLog] prev Addr: %u(0x%X)", info->header.prevAddr, info->header.prevAddr);
        MAIN_TAG_DBG("[CVChasingLog] data length: %u", info->header.dataLength);
    }

    /* Try format API */
    if (CVChasingLog.format())
    {
        if (CVChasingLog.read(&r_data))
        {
            MAIN_TAG_DBG("[CVChasingLog] r_data.a = %u", r_data.a);
        }
        else
        {
            MAIN_TAG_DBG("[CVChasingLog] no Data [TRUE]");
        }
    }
}

// Blinking rate in milliseconds
#define BLINKING_RATE     500ms


int main()
{
    // Initialise the digital pin LED1 as an output
    DigitalOut led(LED1);

    MAIN_TAG_DBG("[setup] CVChasingLog setCallbacks");
    CVChasingLog.setCallbacks(new FlashHandler);

    MAIN_TAG_DBG("[setup] CVChasingLog begin");
    if (CVChasingLog.begin(true))
    {
        fwl_testCase();
    }
    else
    {
        MAIN_TAG_DBG("[setup] CVChasingLog begin failed!");
    }

    while (true) {
        led = !led;
        ThisThread::sleep_for(BLINKING_RATE);
    }
}
