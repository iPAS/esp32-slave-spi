
The code was adopted from https://gist.github.com/shaielc/e0937d68978b03b2544474b641328145.
Thank you to @shaielc for the scaffolding.
Then I found it works a bit currently.
However, the adaptation is still required so a SPI-Slave module is functional
(_transfer data via DMA is disable_).

The main idea is only to connect between SPI-Master signals and of the Slave. 

To test the code, please wiring as the following figure:
![wriing](figure/connection.jpg)

The connections are described:

<table> <tr><td>

| Signal | Pin# |
|:------:|:----:|
| MO     | 22   |
| MI     | 23   |
| MCLK   | 19   |
| MS     | 18   |
| SO     | 32   |
| SI     | 25   |
| SCLK   | 27   |
| SS     | 34   |

<td>

![lolin32](figure/lolin32.png)

</tr></table>

I also use the BusPirate for debugging.
It is just an option, can be neglected.
But, if you have one, 
    [SPI-sniffer](http://dangerousprototypes.com/docs/Bus_Pirate_binary_SPI_sniffer_utility) 
    article and the following BusPirate pinout maybe help:

![buspirate pinout](figure/buspirate.png)

Cheer!
iPAS

