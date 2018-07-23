#ifndef SLAVE_SPI_CLASS
#define SLAVE_SPI_CLASS

#include "Arduino.h"
#include "driver/spi_slave.h"

#define SPI_QUEUE_SIZE 1
#define SPI_MODE 0
#define SPI_DMA 0  // XXX: Still fail of use DMA whether 1 or 2 

#define SPI_MALLOC_CAP (MALLOC_CAP_DMA | MALLOC_CAP_32BIT)
    
void setupIntr(spi_slave_transaction_t * trans);
void transIntr(spi_slave_transaction_t * trans);

class SlaveSPI {

  private:
    static SlaveSPI ** SlaveSPIVector;
    static int         size;

    friend void setupIntr(spi_slave_transaction_t * trans);
    friend void transIntr(spi_slave_transaction_t * trans);

    String buff;        // used to save incoming data
    String transBuffer; // used to buffer outgoing data !not tested!
    size_t t_size;      // length of transaction buffer, (should be set to maximum transition size)

    spi_host_device_t spi_host;  // HSPI, VSPI 

    spi_slave_transaction_t * driver;
    void (*exter_intr)();  // interrupt at the end of transmission ,
                           //  if u need to do something at the end of each transmission

  public:
    SlaveSPI(spi_host_device_t spi_host=HSPI_HOST);  // HSPI, VSPI

    void setup_intr(spi_slave_transaction_t * trans);  // called when the trans is set in the queue
    void trans_intr(spi_slave_transaction_t * trans);  // called when the trans has finished

    void begin(gpio_num_t so, gpio_num_t si, gpio_num_t sclk, gpio_num_t ss, size_t length = 128, void (*ext)() = NULL);
    void trans_queue(String & transmission);  // used to queue data to transmit

    inline char * operator[](int i) {
        return &buff[i];
    }

    inline void flush() {
        buff = "";
    }

    inline bool match(spi_slave_transaction_t * trans);

    inline String * getBuff() {
        return &buff;
    }

    void setDriver();
    char read();
};

#endif