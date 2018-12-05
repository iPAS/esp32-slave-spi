#ifndef __SLAVE_SPI_CLASS_H__
#define __SLAVE_SPI_CLASS_H__

#include <Arduino.h>
#include <SPI.h>
#include <driver/spi_slave.h>

// #define GPIO_HANDSHAKE 

#define SPI_QUEUE_SIZE 1
#define SPI_MODE       SPI_MODE0
#define SPI_DMA        0  // XXX: Still fail of use DMA whether 1 or 2. Don't know why. 

#define SPI_DEFAULT_MAX_BUFFER_SIZE 128

#define SPI_MALLOC_CAP (MALLOC_CAP_DMA | MALLOC_CAP_32BIT)
// #define SPI_MALLOC_CAP (MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT)

#include "SimpleArray.h"
typedef SimpleArray<uint8_t, int> array_t;


class SlaveSPI {

    friend void call_matcher_after_queueing(spi_slave_transaction_t * trans);
    friend void call_matcher_after_transmission(spi_slave_transaction_t * trans);

  private:
    static SlaveSPI ** SlaveSPIVector;
    static int         vector_size;
    
    array_t input_stream  = array_t(SPI_DEFAULT_MAX_BUFFER_SIZE);  // Used to save incoming data
    array_t output_stream = array_t(SPI_DEFAULT_MAX_BUFFER_SIZE);  // Used to buffer outgoing data

    size_t max_buffer_size;  // Length of transaction buffer (maximum transmission size)
    uint8_t * tx_buffer;
    uint8_t * rx_buffer;

    spi_host_device_t spi_host;  // HSPI, VSPI

    spi_slave_transaction_t * transaction;
    int (*callback_after_transmission)();  // Interrupt at the end of transmission,
                                           //   if u need to do something at the end of each transmission
    static int callbackDummy() { return 0; }

  public:
    SlaveSPI(spi_host_device_t spi_host = HSPI_HOST);  // HSPI, VSPI
    esp_err_t initTransmissionQueue();

    void callbackAfterQueueing(spi_slave_transaction_t * trans);      // Called when the trans is set in the queue
    void callbackAfterTransmission(spi_slave_transaction_t * trans);  // Called when the trans has finished

    inline bool match(spi_slave_transaction_t * trans) { return (this->transaction == trans); };

    esp_err_t begin(gpio_num_t so, gpio_num_t si, gpio_num_t sclk, gpio_num_t ss,
                    size_t buffer_size = SPI_DEFAULT_MAX_BUFFER_SIZE, int (*callback)() = callbackDummy);

    void writeFromArray(array_t & array);  // Queue data then wait for transmission
    void readToArray(array_t & array);
    int  readToBytes(void * buf, int size);
    uint8_t readByte();
    
    inline array_t * getInputStream() { return &input_stream; }
    inline void      flushInputStream() { input_stream.clear(); }
};


/**
 * XXX: quick_fix_spi_timing:
 * 
 * The recceived data from MISO are shifted by one bit in every byte. 
 * Helped by https://github.com/espressif/arduino-esp32/issues/1427
 */
#include <soc/spi_struct.h>
struct spi_struct_t {
    spi_dev_t * dev;
    #if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
    #endif
    uint8_t num;
};

void quick_fix_spi_timing(spi_t * _spi);


/**
 * Auxiliary:
 */
#if (0)
#define DEBUG_PRINT(str)                \
    Serial.print(F("["));               \
    Serial.print(__FILE__);             \
    Serial.print(F(" > "));             \
    Serial.print(__PRETTY_FUNCTION__);  \
    Serial.print(F(' @'));              \
    Serial.print(__LINE__);             \
    Serial.print(' : ');                \
    Serial.println(str);
#else
#define DEBUG_PRINT(str) {       \
    Serial.print(F("["));        \
    Serial.print(__FUNCTION__);  \
    Serial.print(F("@"));        \
    Serial.print(__LINE__);      \
    Serial.print(": ");          \
    Serial.println(str); }
#endif


#endif  // #ifndef __SLAVE_SPI_CLASS_H__
