/**
 * SlaveSPI Class from
 * - [shaielc/SlaveSPIClass.cpp](https://gist.github.com/shaielc/e0937d68978b03b2544474b641328145)
 */
#include "SlaveSPIClass.h"

#include <SPI.h>

#define MO   22
#define MI   23
#define MCLK 19
#define MS   18

#define SO   (gpio_num_t)32
#define SI   (gpio_num_t)25
#define SCLK (gpio_num_t)27
#define SS   (gpio_num_t)34

SPIClass master(VSPI);
SPISettings spi_setting(100000, MSBFIRST, SPI_MODE);

SlaveSPI slave(HSPI_HOST);

static String txt = "";
static String cmd = "";

void slave_tx_finish() {
    // Serial.println("[slave_tx_finish] slave transmission has been finished!");
    // Serial.println(slave[0]);
}

void print_hex(String str) {
    for (int i = 0; i < str.length(); i++) {
        Serial.print(str[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    
    master.begin(MCLK, MI, MO);

    pinMode(MS, OUTPUT);
    digitalWrite(MS, HIGH);
    // slave.begin(SO, SI, SCLK, SS, 8, slave_tx_finish);  // seems to work with groups of 4 bytes
    // slave.begin(SO, SI, SCLK, SS, 4, slave_tx_finish);
    slave.begin(SO, SI, SCLK, SS, 2, slave_tx_finish);
    // slave.begin(SO, SI, SCLK, SS, 1, slave_tx_finish);  // at least 2 word in an SPI frame
}

void loop() {
    if (slave.getBuff()->length() && digitalRead(SS) == HIGH) {  // Slave SPI has got data in.
        while (slave.getBuff()->length()) { 
            txt += slave.read();
        }
        Serial.print("slave input: ");
        print_hex(txt);
    }

    while (Serial.available()) {  // Serial has got data in 
        cmd += (char)Serial.read();
    }

    while (txt.length() > 0) {  // Slave SPI output
        slave.trans_queue(txt);
        Serial.print("slave output: ");
        print_hex(txt);
        txt = "";
    }

    while (cmd.length() > 0) {  // From serial to Master SPI
        Serial.print("serial input / master output: ");
        Serial.println(cmd);

        Serial.print("master input (whether read or write mode): ");
        
        digitalWrite(MS, LOW);
        master.beginTransaction(spi_setting);
        for (int i = 0; i < cmd.length(); i++) {
            cmd[i] = master.transfer(cmd[i]);  // ERROR : gives the transmitted data <<1
        }
        master.endTransaction();
        digitalWrite(MS, HIGH);
        
        print_hex(cmd);
        cmd = "";
    }
}
