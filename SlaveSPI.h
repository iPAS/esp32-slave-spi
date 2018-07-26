#ifndef SLAVE_SPI_CLASS
#define SLAVE_SPI_CLASS

#include "Arduino.h"
#include "driver/spi_slave.h"

#define SPI_QUEUE_SIZE 1
#define SPI_MODE 0
#define SPI_DMA 0  // XXX: Still fail of use DMA whether 1 or 2 

#define SPI_DEFAULT_MAX_BUFFER_SIZE 128

// #define SPI_MALLOC_CAP (MALLOC_CAP_DMA | MALLOC_CAP_32BIT)
#define SPI_MALLOC_CAP (MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT)

class SlaveSPI {

    friend void call_matcher_after_queueing(spi_slave_transaction_t * trans);
    friend void call_matcher_after_transmission(spi_slave_transaction_t * trans);

  private:
    static SlaveSPI ** SlaveSPIVector;
    static int         vector_size;

    String input_stream;     // Used to save incoming data
    String output_stream;    // Used to buffer outgoing data
    size_t max_buffer_size;  // Length of transaction buffer (maximum transmission size)

    spi_host_device_t spi_host;  // HSPI, VSPI

    byte * tx_buffer;
    byte * rx_buffer;

    spi_slave_transaction_t * transaction;
    int (*callback_after_transmission)();  // Interrupt at the end of transmission,
                                           //   if u need to do something at the end of each transmission
    static int callbackDummy() { return 0; }

  public:
    SlaveSPI(spi_host_device_t spi_host=HSPI_HOST);  // HSPI, VSPI

    void callbackAfterQueueing(spi_slave_transaction_t * trans);      // Called when the trans is set in the queue
    void callbackAfterTransmission(spi_slave_transaction_t * trans);  // Called when the trans has finished

    inline bool match(spi_slave_transaction_t * trans);
    void initTransmissionQueue();

    void begin(gpio_num_t so, gpio_num_t si, gpio_num_t sclk, gpio_num_t ss,
               size_t buffer_size = SPI_DEFAULT_MAX_BUFFER_SIZE, int (*callback)() = callbackDummy);

    void write(String & msg);  // Queue data then wait for transmission
    byte read();

    inline char *   operator[](int i) { return &input_stream[i]; }
    inline String * getInputStream() { return &input_stream; }
    inline void     flushInputStream() { input_stream = ""; }
};

#endif