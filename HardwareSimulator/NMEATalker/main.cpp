// 
// main.cpp generated by embedXcode
// from /Users/backuplocal/Library/Arduino15/packages/esp32/hardware/esp32/1.0.4/cores/esp32/main.cpp
// ----------------------------------
// DO NOT EDIT THIS FILE.
// ----------------------------------
#if defined(EMBEDXCODE)
 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "Arduino.h"

TaskHandle_t loopTaskHandle = NULL;

#if CONFIG_AUTOSTART_ARDUINO

bool loopTaskWDTEnabled;

void loopTask(void *pvParameters)
{
    setup();
    for(;;) {
        if(loopTaskWDTEnabled){
            esp_task_wdt_reset();
        }
        loop();
    }
}

extern "C" void app_main()
{
    loopTaskWDTEnabled = false;
    initArduino();
    xTaskCreateUniversal(loopTask, "loopTask", 8192, NULL, 1, &loopTaskHandle, CONFIG_ARDUINO_RUNNING_CORE);
}

#endif
 
#include "NMEATalker.ino"
 
#endif // EMBEDXCODE
