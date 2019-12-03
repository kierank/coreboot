# X11 LGA1151v2 series

The Supermicro X11 LGA1151 v2 series boards all have a C240 series PCH, but have differences
regarding internal and external interfaces, such as available PCIe slots, NICs (1 GbE, 10 GbE),
IPMI, RAID controller, etc.

## Supported boards

- [X11SCH-F](x11sch-f/x11sch-f.md)

## Required proprietary blobs

- [Intel FSP2.0]
- Intel ME/SPS

## De-blobbing

- [Intel FSP2.0] can not be removed as long as there is no free replacement.

## Flashing coreboot

Look at the [flashing tutorial] and the board-specific section.

## Known issues

These issues apply to all boards. Have a look at the board-specific issues, too.

- MRC fast boot does not work on cold boot with Intel SPS (see [Intel FSP2.0]).

## To do

- Fix issues above
- Fix issues in board specific sections
- Fix FIXMEs and TODOs in code
- Add more boards! :-)

## Technology

```eval_rst
+------------------+--------------------------------------------------+
| CPU              | Intel Coffee Lake                                |
+------------------+--------------------------------------------------+
| PCH              | Intel C242/C246                                  |
+------------------+--------------------------------------------------+
| Coprocessor      | Intel SPS (server version of the ME)             |
+------------------+--------------------------------------------------+
```

## Extra links

[OpenBMC]: https://www.openbmc.org/
[flashrom]: https://flashrom.org/Flashrom
[flashing tutorial]: ../../../../flash_tutorial/ext_power.md
[Intel FSP2.0]: ../../../../soc/intel/fsp/index.md
[AST2500]: https://www.aspeedtech.com/products.php?fPath=20&rId=440
