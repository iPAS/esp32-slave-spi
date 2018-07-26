#include "SlaveSPI.h"

int         SlaveSPI::size           = 0;
SlaveSPI ** SlaveSPI::SlaveSPIVector = NULL;

void setupIntr(spi_slave_transaction_t * trans) {
    for (int i = 0; i < SlaveSPI::size; i++)
        if (SlaveSPI::SlaveSPIVector[i]->match(trans)) SlaveSPI::SlaveSPIVector[i]->setup_intr(trans);
}

void transIntr(spi_slave_transaction_t * trans) {
    for (int i = 0; i < SlaveSPI::size; i++)
        if (SlaveSPI::SlaveSPIVector[i]->match(trans)) SlaveSPI::SlaveSPIVector[i]->trans_intr(trans);
}

SlaveSPI::SlaveSPI(spi_host_device_t spi_host) {
    this->spi_host = spi_host;

    SlaveSPI ** temp = new SlaveSPI *[size + 1];  // Create a new instance array
    for (int i = 0; i < size; i++)                // Relocate all instances into the new array
        temp[i] = SlaveSPIVector[i];

    temp[size++] = this;  // Put this instance into

    delete[] SlaveSPIVector;  // Delete the old one
    SlaveSPIVector = temp;    // Point to the new one

    buff        = "";
    transBuffer = "";
}

void SlaveSPI::begin(gpio_num_t so, gpio_num_t si, gpio_num_t sclk, gpio_num_t ss, size_t length, void (*ext)()) {
    t_size   = length;  // should set to the minimum transaction length
    txBuffer = (byte *)heap_caps_malloc(max(t_size, 32), SPI_MALLOC_CAP);
    rxBuffer = (byte *)heap_caps_malloc(max(t_size, 32), SPI_MALLOC_CAP);
    memset(txBuffer, 0, t_size);
    memset(rxBuffer, 0, t_size);

    driver = new spi_slave_transaction_t{
        .length    = t_size * 8,  //< Total data length, in bits
        .trans_len = 0,           //< Transaction data length, in bits
        .tx_buffer = txBuffer,    //< tx buffer, or NULL for no MOSI phase
        .rx_buffer = rxBuffer,    //< rx buffer, or NULL for no MISO phase
        .user      = NULL         //< User-defined variable. Can be used to store e.g. transaction ID.
    };

    spi_bus_config_t buscfg = {.mosi_io_num = si, .miso_io_num = so, .sclk_io_num = sclk};

    gpio_set_pull_mode(sclk, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(ss,   GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(si,   GPIO_PULLUP_ONLY);
    spi_slave_interface_config_t slvcfg = {
        .spics_io_num = ss,              //< CS GPIO pin for this device
        .flags        = 0,               //< Bitwise OR of SPI_SLAVE_* flags
        .queue_size   = SPI_QUEUE_SIZE,  //< Transaction queue size.
                                         //  This sets how many transactions can be 'in the air'
                                         //    (queued using spi_slave_queue_trans but not yet finished
                                         //    using spi_slave_get_trans_result) at the same time
        .mode          = SPI_MODE,       //< SPI mode (0-3)
        .post_setup_cb = setupIntr,      //< Callback called after the SPI registers are loaded with new data
        .post_trans_cb = transIntr       //< Callback called after a transaction is done
    };

    exter_intr = ext;

    esp_err_t err;
    if (err = spi_slave_initialize(spi_host, &buscfg, &slvcfg, SPI_DMA)) {
        Serial.print(F("[SlaveSPI::begin] spi_slave_initialize err: "));
        Serial.println(err);
    }
    if (err = spi_slave_queue_trans(spi_host, driver, portMAX_DELAY)) {  // ready for input (but no transmit)
        Serial.print(F("[SlaveSPI::begin] spi_slave_queue_trans err: "));
        Serial.println(err);
    }
}

void SlaveSPI::setup_intr(spi_slave_transaction_t * trans) {  // called when the trans is set in the queue
                                                              // didn't find use for it in the end.
}

void SlaveSPI::trans_intr(spi_slave_transaction_t * trans) {  // called when the trans has finished
    for (int i = 0; i < t_size; i++) {
        buff += ((char *)driver->rx_buffer)[i];    // Copy
        ((char *)driver->rx_buffer)[i] = (char)0;  // Clean
    }
    setDriver();
    exter_intr();
}

void SlaveSPI::trans_queue(String & transmission) {  // used to queue data to transmit
    for (int i = 0; i < transmission.length(); i++) transBuffer += transmission[i];
}

inline bool SlaveSPI::match(spi_slave_transaction_t * trans) {
    return (this->driver == trans);
}

void SlaveSPI::setDriver() {
    driver->user = NULL;

    int i = 0;
    for (; i < t_size && i < transBuffer.length(); i++) 
        ((char *)driver->tx_buffer)[i] = transBuffer[i];    

    transBuffer       = &(transBuffer[i]);
    driver->length    = t_size * 8;
    driver->trans_len = 0;
    spi_slave_queue_trans(spi_host, driver, portMAX_DELAY);
}

char SlaveSPI::read() {
    char temp = buff[0];
    buff.remove(0, 1);
    return temp;
}
