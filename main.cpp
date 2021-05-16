#include "mbed.h"
#include "FlashWearLevellingUtils.h"

// Blinking rate in milliseconds
#define BLINKING_RATE     500ms

#define MAIN_TAG_INFO(f_, ...) printf("\r\n[MAIN ] " f_, ##__VA_ARGS__)

typedef struct {
    uint8_t a[10];
} flash_data_t;

/* Declare object flash data type handler */
FlashWearLevellingUtils<flash_data_t> CVChasingLog(0, (1024 * 1024), 512);

/* Register callback handler external flash memory */
class FlashHandler: public FlashWearLevellingCallbacks {
    bool onRead(uint8_t* buff, size_t* length) {
        MAIN_TAG_INFO("flash onRead data with length %u", *length);
        return true;
    }

    bool onWrite(uint8_t* buff, size_t* length) {
        MAIN_TAG_INFO("flash onWrite data with length %u", *length);
        return true;
    }

    bool onErase(uint32_t page) {
        MAIN_TAG_INFO("flash onErase page %u", page);
        return true;
    }

    bool onReady() {
        MAIN_TAG_INFO("flash onReady");
        return true;
    }

    void onStatus(status_t s) {
        MAIN_TAG_INFO("flash onStatus %u", (uint8_t)s);
    }
};

int main()
{
    flash_data_t w_data;
    flash_data_t r_data;

    // Initialise the digital pin LED1 as an output
    DigitalOut led(LED1);

    CVChasingLog.setCallbacks(new FlashHandler);
    CVChasingLog.begin();
    CVChasingLog.write(&w_data);
    CVChasingLog.read(&r_data);

    while (true) {
        led = !led;
        ThisThread::sleep_for(BLINKING_RATE);
    }
}
