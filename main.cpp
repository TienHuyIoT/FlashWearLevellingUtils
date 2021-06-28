#include "mbed.h"
#include "FlashWearLevellingUtils.h"
#include "console_dbg.h"

/* Private macro -------------------------------------------------------------*/
#define MAIN_TAG_DBG(...) CONSOLE_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define FWL_TAG_CALLBACK(...) CONSOLE_TAG_LOGI("[FWL_CB]", __VA_ARGS__)
#define MAIN_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)

typedef struct {
    uint32_t a;
} flash_data_t;

/* Declare object flash data type handler */
FlashWearLevellingUtils u32_log(0, 256, 64, sizeof(flash_data_t));

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
        FWL_TAG_CALLBACK("[FlashHandler] onRead data [addr][length]: [%u(0x%x)][%u]", addr, addr, *length);
        return v_testAPI.read(addr, buff, length);
    }

    bool onWrite(uint32_t addr, uint8_t *buff, uint16_t *length) {
        FWL_TAG_CALLBACK("[FlashHandler] onWrite data [addr][length]: [%u(0x%x)][%u]", addr, addr, *length);
        return v_testAPI.write(addr, buff, length);
    }

    bool onErase(uint32_t addr, uint16_t length) {
        FWL_TAG_CALLBACK("flash onErase [addr][length]: [%u(0x%x)][%u]", addr, addr, length);
        return v_testAPI.erase(addr, length);
    }

    bool onReady() {
        FWL_TAG_CALLBACK("[FlashHandler] onReady");
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
        if (!u32_log.write(&w_data))
        {
            MAIN_TAG_DBG("[u32_log] read data failed!");
            break;
        }
    }

    if (u32_log.read(&r_data))
    {
        MAIN_TAG_DBG("[u32_log] r_data.a = %u", r_data.a);

        memory_cxt_t info = u32_log.info();
        MAIN_TAG_DBG("[u32_log] Addr: %u(0x%X)", info.header.addr, info.header.addr);
        MAIN_TAG_DBG("[u32_log] crc32: 0x%X", info.header.crc32);
        MAIN_TAG_DBG("[u32_log] next Addr: %u(0x%X)", info.header.nextAddr, info.header.nextAddr);
        MAIN_TAG_DBG("[u32_log] prev Addr: %u(0x%X)", info.header.prevAddr, info.header.prevAddr);
        MAIN_TAG_DBG("[u32_log] data length: %u", info.header.dataLength);
    }

    /* Try format API */
    if (u32_log.format())
    {
        if (u32_log.read(&r_data))
        {
            MAIN_TAG_DBG("[u32_log] r_data.a = %u", r_data.a);
        }
        else
        {
            MAIN_TAG_DBG("[u32_log] no Data [TRUE]");
        }
    }
}

// Blinking rate in milliseconds
#define BLINKING_RATE     500ms


int main()
{
    // Initialise the digital pin LED1 as an output
    DigitalOut led(LED1);

    MAIN_TAG_DBG("[setup] u32_log setCallbacks");
    u32_log.setCallbacks(new FlashHandler);

    MAIN_TAG_DBG("[setup] u32_log begin");
    if (u32_log.begin(true))
    {
        fwl_testCase();
    }
    else
    {
        MAIN_TAG_DBG("[setup] u32_log begin failed!");
    }

    while (true) {
        led = !led;
        ThisThread::sleep_for(BLINKING_RATE);
    }
}
