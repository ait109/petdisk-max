#include "D64DataSource.h"
#include "fsDataSource.h"
#include "ConsoleLogger.h"

int main(int argc, char** argv)
{
    D64DataSource d64ds;
    fsDataSource fsds;
    ConsoleLogger logger;
    logger.log("test\n");

    fsds.init();
    d64ds.initWithDataSource(&fsds, "test.d64", &logger);

    char fname[21];
    for (int i = 0; i < 10; i++)
    {
        sprintf(fname, "FNAME%d.PRG", i);
        d64ds.openFileForWriting((unsigned char*)fname);

        uint8_t data[1024];
        uint8_t* dptr = data;
        memset(data, 0xBB, 1024);

        int remainingBytes = 1024;
        while (remainingBytes > 0)
        {
            int bytesToSend = d64ds.writeBufferSize();
            if (remainingBytes < bytesToSend)
            {
                bytesToSend = remainingBytes;
            }

            memcpy(d64ds.getBuffer(), dptr, bytesToSend);
            dptr += bytesToSend;
            remainingBytes -= bytesToSend;

            d64ds.writeBufferToFile(bytesToSend);
        }

        d64ds.closeFile();
    }

    d64ds.openCurrentDirectory();
    while (d64ds.getNextDirectoryEntry())
    {
        printf("f: %s\n", d64ds.getFilename());
    }

    logger.log("1234\n");
}