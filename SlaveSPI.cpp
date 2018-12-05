/**
 * SlaveSPI
 * 
 *  Created on: Jul 1, 2018
 *       Email: ipas.th@gmail.com
 *      Author: ipas 
 */

#include "SlaveSPI.h"

int         SlaveSPI::vector_size    = 0;
SlaveSPI ** SlaveSPI::SlaveSPIVector = NULL;

void call_matcher_after_queueing(spi_slave_transaction_t * trans) {  // Call the hook matched to its transaction.
    for (int i = 0; i < SlaveSPI::vector_size; i++)
        if (SlaveSPI::SlaveSPIVector[i]->match(trans)) SlaveSPI::SlaveSPIVector[i]->callbackAfterQueueing(trans);
}

void call_matcher_after_transmission(spi_slave_transaction_t * trans) {  // Call the hook matched to its transaction.
    for (int i = 0; i < SlaveSPI::vector_size; i++)
        if (SlaveSPI::SlaveSPIVector[i]->match(trans)) SlaveSPI::SlaveSPIVector[i]->callbackAfterTransmission(trans);
}

/**
 * Constructor 
 */
SlaveSPI::SlaveSPI(spi_host_device_t spi_host) {
    this->spi_host = spi_host;

    SlaveSPI ** temp = new SlaveSPI *[vector_size + 1];  // Create a new instance array
    for (int i = 0; i < vector_size; i++) {              // Relocate all instances into the new array
        temp[i] = SlaveSPIVector[i];
    }
    temp[vector_size++] = this;  // Put this instance into
    delete[] SlaveSPIVector;     // Delete the old one
    SlaveSPIVector = temp;       // Point to the new one

    output_stream.clear();
    input_stream.clear();
}

/**
 * To initialize the Slave-SPI module.
 */ 
esp_err_t SlaveSPI::begin(gpio_num_t so, gpio_num_t si, gpio_num_t sclk, gpio_num_t ss, size_t buffer_size, int (*callback)()) {
    callback_after_transmission = callback;

    max_buffer_size = buffer_size;  // should set to the minimum transaction length
    tx_buffer       = (uint8_t *)heap_caps_malloc(max(max_buffer_size, 32), SPI_MALLOC_CAP);
    rx_buffer       = (uint8_t *)heap_caps_malloc(max(max_buffer_size, 32), SPI_MALLOC_CAP);
    
    // for (int i = 0; i < max_buffer_size; i++) { tx_buffer[i] = 0; rx_buffer[i] = 0; }  // XXX: memset
    memset(tx_buffer, 0, max_buffer_size);
    memset(rx_buffer, 0, max_buffer_size);
    
    /*
    Initialize the Slave-SPI module:
    */

    // Configuration for the handshake line
    // TODO:
    // gpio_config_t io_conf = {.intr_type = GPIO_INTR_DISABLE, .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = (1 << GPIO_HANDSHAKE)};
    // gpio_config(&io_conf);

    spi_bus_config_t buscfg = {.mosi_io_num = si, .miso_io_num = so, .sclk_io_num = sclk};
    gpio_set_pull_mode(sclk, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(ss,   GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(si,   GPIO_PULLUP_ONLY);

    spi_slave_interface_config_t slvcfg = {
        .spics_io_num = ss,              //< CS GPIO pin for this device
        .flags        = 0,               //< Bitwise OR of SPI_SLAVE_* flags
        .queue_size   = SPI_QUEUE_SIZE,  //< Transaction queue size.
                                         //  This sets how many transactions can be 'in the air' (
                                         //    queued using spi_slave_queue_trans -- non-blocking --
                                         //    but not yet finished using spi_slave_get_trans_result -- blocking)
                                         //    at the same time
        .mode          = SPI_MODE,       //< SPI mode (0-3)
        .post_setup_cb = call_matcher_after_queueing,     //< Called after the SPI registers are loaded with new data
        .post_trans_cb = call_matcher_after_transmission  //< Called after a transaction is done
    };

    esp_err_t err = spi_slave_initialize(spi_host, &buscfg, &slvcfg, SPI_DMA);  // Setup the SPI module
    if (err != ESP_OK) {
        DEBUG_PRINT(err);
        return err;
    }

    /*
    Prepare transaction queue:

    The amount of data written to the buffers is limited by the __length__ member of the transaction structure: 
        the driver will never read/write more data than indicated there. 
    The __length__ cannot define the actual length of the SPI transaction, 
        this is determined by the master as it drives the clock and CS lines. 
    
    The actual length transferred can be read from the __trans_len__ member 
        of the spi_slave_transaction_t structure after transaction. 
    
    In case the length of the transmission is larger than the buffer length,
        only the start of the transmission will be sent and received, 
        and the __trans_len__ is set to __length__ instead of the actual length. 
    It's recommended to set __length__ longer than the maximum length expected if the __trans_len__ is required. 
    
    In case the transmission length is shorter than the buffer length, only data up to the length of the buffer will be exchanged.
    */
    transaction = new spi_slave_transaction_t{
        .length    = max_buffer_size << 3,  //< Total data length, in bits (x8) -- maximum receivable
        .trans_len = 0,                     //< Transaction data length, in bits -- actual received
        .tx_buffer = tx_buffer,             //< tx buffer, or NULL for no MOSI phase
        .rx_buffer = rx_buffer,             //< rx buffer, or NULL for no MISO phase
        .user      = NULL                   //< User-defined variable. Can be used to store e.g. transaction ID.
    };
    
    err = initTransmissionQueue();
    if (err != ESP_OK) {
        DEBUG_PRINT(err);
    }
    return err;
}

/**
 * Called after a transaction is queued and ready for pickup by master. 
 */
void SlaveSPI::callbackAfterQueueing(spi_slave_transaction_t * trans) {  
    // TODO: data ready -- trig hand-check pin to high
    // WRITE_PERI_REG(GPIO_OUT_W1TS_REG, (1<<GPIO_HANDSHAKE));
}

/**
 * Called after transaction is sent/received. 
 */
void SlaveSPI::callbackAfterTransmission(spi_slave_transaction_t * trans) {
    // Aggregate in-comming to input_stream
    for (int i = 0; i < (transaction->trans_len >> 3); i++) {  // Copy by actual received. Div by 8, to byte
        input_stream += ((char *)transaction->rx_buffer)[i];   // Aggregate
        ((char *)transaction->rx_buffer)[i] = 0;               // Clean
    }

    // Setup for receiving buffer. Prepare for next transaction
    transaction->trans_len = 0;     // Set zero on slave's actual received data.
    transaction->user      = NULL;  // XXX: reset?

    esp_err_t err = initTransmissionQueue();  // Prepare the out-going queue and initialize the transmission configuration
    if (err != ESP_OK) {
        DEBUG_PRINT(err);
        return;
    }

    int ret = callback_after_transmission();  // Callback to user function hook

    // TODO: transmission succeed -- trig hand-check pin to low
    // WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1<<GPIO_HANDSHAKE));
}

esp_err_t SlaveSPI::initTransmissionQueue() {
    // Prepare out-going data for next request by the master
    // int i = 0;
    // for (; i < max_buffer_size && i < output_stream.length(); i++) {  // NOT over buffer size
    //     ((char *)transaction->tx_buffer)[i] = output_stream[i];       // Copy prepared data to out-going queue
    // }
    // output_stream = &(output_stream[i]);  // Segmentation. The remain is left for future.

    int size = min(max_buffer_size, output_stream.length());                  // NOT over the buffer's size.
    memcpy((void *)transaction->tx_buffer, output_stream.getBuffer(), size);  // Rearrange the tx data.
    output_stream.remove(0, size);                                            // Segmentation. Remain for future.

    // Queue. Ready for sending if receiving
    return spi_slave_queue_trans(spi_host, transaction, portMAX_DELAY);
}

/**
 * To read/write SPI queue data.
 */ 
void SlaveSPI::writeFromArray(array_t & array) {  // used to queue data to transmit
    output_stream += array;
}

void SlaveSPI::readToArray(array_t & array) {  // move read data into buf
    array += input_stream;
    input_stream.clear();
}

int SlaveSPI::readToBytes(void * buf, int size) {
    int ret = input_stream.getBytes(buf, size);
    input_stream.clear();
    return ret;
}

uint8_t SlaveSPI::readByte() {
    uint8_t tmp = input_stream[0];
    input_stream.remove(0, 1);
    return tmp;
}

/**
 * Quick fix the SPI timeing problem that make received bits are shift left by one.
 */
void quick_fix_spi_timing(spi_t * _spi) { _spi->dev->ctrl2.miso_delay_mode = 2; }
