# Arduino-EeValues
For Arduino -- Store records into EEMEM.  The ATMEL 8-bit processors have from 128 to 2048 bytes of non-volatile storage that is byte addressable.  The constant `E2END` is the last byte addressable.

In the header of each record is a 4-char identification, size of the record ( sum of header overhead and user data ), and a CRC.  Several can be stored at different offsets within EEMEM.  This allows you a small file-store like non-volatile storage.  If you want to do wear-leveling, store the record at different offsets.  The library can search for a valid header (just remember to erase the previous record).

This is a library for Arduino IDE.  It was tested on IDE verison 1.5.2 under Win7  and run on an UNO and a Leonardo.

If you have suggestions, and want to fork the repo or send pull requests, please do!

# Downloading
When you download, you'll get a file "Arduino-EeValues-master.zip".  Extract to your ${HOME}/Arduino/libraries/ folder, and rename  "Arduino-EeValues-Master" to "EeValues".  Now start the IDE , go to `File > Sketchbook > libraries` to add EeValues.  Once added, you can access the example sketch.

# Using Library In Your Application
There is an oddity about the Arduino IDE: a library cannot depend on another library.  In this case EeValues wants to reference the Crc8 library like this:
`#include <Crc8.h>`
However, when compiling the EeValues library, `Arduino/libraries` isn't added to the *System Include Folder List*.  So two things are required:

0. The application must `#include <crc8.h>` to create the functions, and
0. The library includes this library with the hack `#include "../crc8/crc8.h"`.  
See [Arduino Build Process](https://code.google.com/p/arduino/wiki/BuildProcess) for details.


# This Library Depends On...
It compiles nicely ; however, it requires two other libraries:

0. crc8 ( Crc8 )
0. CrunchyNoodles Utilities ( CnUtils ) , for debugging.

Both of these are available in my user area on (https://github.com/sacnorthern/)[SacNOrthern's GitHub].

In the near-future, I want to make improvements.  I would like to change to CRC-16 from CRC-8, since the CRC-16 library is already  part of the IDE.


# License
Copyright (C) 2013, 2015 Brian Witt.
Apache License Version 2.0, January 2004 


Have a nice day.

