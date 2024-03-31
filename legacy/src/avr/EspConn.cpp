#include "../EspConn.h"
#include <stdio.h>
#include <ctype.h>
#include <util/delay.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/pgmspace.h>

#define NUMESPTAGS 5

const char* ESPTAGS[] =
{
    "\r\nOK\r\n",
    "\r\nERROR\r\n",
    "\r\nFAIL\r\n",
    "\r\nSEND OK\r\n",
    " CONNECT\r\n"
};

typedef enum
{
    STATE_READ = 0,
    STATE_O = 1,
    STATE_K = 2,
    STATE_NL_1 = 3,
    STATE_NL_2 = 4,
} readState;

namespace bitfixer {

bool EspConn::initWithParams(uint8_t* buffer, uint16_t* bufferSize, Serial* serial, Logger* logSerial) {
    _serialBuffer = buffer;
    _serialBufferSize = bufferSize;
    _serial = serial;
    _logSerial = logSerial;

    return true;
}

bool EspConn::device_present() {
    if (!sendCmd("AT", 1000))
    {
        return false;
    }

    return true;
}

bool EspConn::attempt_baud_rate_setting() {
    _serial->init(115200);
    char cmd[64];
    sprintf_P(cmd, PSTR("AT+UART_DEF=500000,8,1,0,0\r\n"));
    _serial->transmitString(cmd);
    _delay_ms(1000);
    _serial->init(500000);
    return true;
}

bool EspConn::init() {
    if (!sendCmd("AT+CWMODE=1", 1000))
    {
        return false;
    }

    if (!sendCmd("AT+CIPMUX=1", 1000))
    {
        return false;
    }

    return true;
}

void EspConn::scanNetworks() {
    sendCmd("AT+CWLAP");
}

void EspConn::setDns() {
    char cmd[64];
    sprintf_P(cmd, PSTR("AT+CIPDNS_CUR=1"));
    sendCmd((const char*)cmd, 5000);
}

void EspConn::copyEscapedString(char* dest, const char* src)
{
    int srcLen = strlen(src);
    char* destptr = dest;

    for (int i = 0; i < srcLen; i++)
    {
        char c = src[i];
        if (isalnum(c))
        {
            *destptr++ = c;
        }
        else
        {
            sprintf(destptr, "\\%c", c);
            destptr += 2;
        }
    }
    destptr = 0x00;
}

bool EspConn::connect(const char* ssid, const char* passphrase) {
    char* buffer = (char*)_serialBuffer;
    char* escapedSsid = buffer + 257;
    char* escapedPassphrase = escapedSsid + 33;

    copyEscapedString(escapedSsid, ssid);
    copyEscapedString(escapedPassphrase, passphrase);
    sprintf_P(buffer, PSTR("AT+CWJAP_CUR=\"%s\",\"%s\""), escapedSsid, escapedPassphrase);
    return sendCmd((const char*)_serialBuffer);
}

bool EspConn::sendCmd(const char* cmd, int timeout) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        *_serialBufferSize = 0;
    }

    _serial->transmitString(cmd);
    _serial->enable_interrupt();
    _serial->transmitString("\r\n");
    int result = readUntil(0, true, true, timeout);
    _serial->disable_interrupt();

    if (result < 0 || result == 2 /* FAIL */)
    {
        return false;
    }
    
    return true;
}

bool EspConn::startClient(const char* host, uint16_t port, uint8_t sock, uint8_t protMode)
{
    //LOGDEBUG2(F("> startClient"), host, port);
    
    // TCP
    // AT+CIPSTART=<link ID>,"TCP",<remote IP>,<remote port>

    // UDP
    // AT+CIPSTART=<link ID>,"UDP",<remote IP>,<remote port>[,<UDP local port>,<UDP mode>]

    // for UDP we set a dummy remote port and UDP mode to 2
    // this allows to specify the target host/port in CIPSEND
    
    int ret = -1;
    char* cmd = (char*)&_serialBuffer[64];
    if (protMode==TCP_MODE) {
        sprintf_P(cmd, PSTR("AT+CIPSTART=%d,\"TCP\",\"%s\",%u"), sock, host, port);
        return sendCmd(cmd);
    }
    else if (protMode==SSL_MODE)
    {
        char tmp[64];
        strcpy(tmp, host);
        // better to put the CIPSSLSIZE here because it is not supported before firmware 1.4
        sprintf_P(cmd, PSTR("AT+CIPSSLSIZE=4096"));
        sendCmd(cmd);

        sprintf_P(cmd, PSTR("AT+CIPSTART=%d,\"SSL\",\"%s\",%u"), sock, tmp, port);
        sendCmd(cmd);
    }
    else if (protMode==UDP_MODE) {
        sprintf_P(cmd, PSTR("AT+CIPSTART=%d,\"UDP\",\"%s\",%u,1112,0"), sock, host, port);
        sendCmd(cmd);
    }
    return ret==TAG_OK;
}

void EspConn::stopClient(uint8_t sock)
{
    char cmd[64];
    sprintf_P(cmd, PSTR("AT+CIPCLOSE=%d"), sock);
    sendCmd(cmd);
}


int contains(const char* big, const char* small, size_t size)
{
    int sm_len = strlen(small);
    int end = size - sm_len;
    for (int i = 0; i <= end; i++)
    {
        if (big[i] == small[0])
        {
            if (memcmp(&big[i], small, sm_len) == 0)
            {
                return i;
            }
        }
    }

    return -1;
}

void EspConn::sendData(uint8_t sock, unsigned char* data, int len)
{
    char* cmd = (char*)&_serialBuffer[64];
    sprintf_P(cmd, PSTR("AT+CIPSEND=%d,%d\r\n"), sock, len);
    
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        *_serialBufferSize = 0;
    }

    _serial->enable_interrupt();
    _serial->transmitString(cmd);
    readUntil("> ", false, true, 5000);

    // send payload data
    for (int i = 0; i < len; i++)
    {
        _serial->transmitByte(data[i]);
    }

    if (sock == 0)
    {
        uint16_t current_buffer_size;
        // keep reading until socket closed
        // socket will close automatically for http 1.0
        readUntil("0,CLOSED\r\n", false, false, 5000);
        _serial->disable_interrupt();

        ATOMIC_BLOCK(ATOMIC_FORCEON) {
            current_buffer_size = *_serialBufferSize;
        }

        // remove IPD messages
        int start = 0;
        do {
            start = contains((const char*)_serialBuffer, (const char*)"\r\n+IPD", current_buffer_size);
            if (start > 0)
            {
                // find end of message
                int end = contains((const char*)&_serialBuffer[start], (const char*)":", current_buffer_size-start);
                
                // move memory to remove ipd message
                current_buffer_size -= end+1;
                memmove(&_serialBuffer[start], &_serialBuffer[start+end+1], current_buffer_size - start);
            }
        } while (start >= 0);
    }
    else
    {
        readUntil(0, true, true, 5000);
        _serial->disable_interrupt();
        //return -1;
    }
}

void EspConn::readBytesUntilSize(uint16_t size)
{
    // keep reading until the right number of bytes has been read
    bool done = false;
    uint16_t current_buffer_size = 0;
    while (!done)
    {
        ATOMIC_BLOCK(ATOMIC_FORCEON) 
        {
            current_buffer_size = *_serialBufferSize;
        }

        if (current_buffer_size >= size)
        {
            done = true;
        }
        else
        {
            _delay_ms(1);
        }
    }
}

bool endsWith(const char* big, const char* small, size_t size)
{
    int start = size - strlen(small);
    for (int i = 0; i < (int)strlen(small); i++)
    {
        if (big[i+start] != small[i])
        {
            return false;
        } 
    }

    return true;
}

int EspConn::readUntil(const char* tag, bool findTags, bool end, int timeout) {
    int ret = -1;
    uint16_t current_buffer_size = 0;
    int currtime = 0;
    while (ret < 0 && currtime < timeout) 
    {
        ATOMIC_BLOCK(ATOMIC_FORCEON) {
            current_buffer_size = *_serialBufferSize;
        }
        if (current_buffer_size > 0) 
        {
            if (tag != 0) 
            {
                if (end)
                {
                    if (endsWith((const char*)_serialBuffer, tag, current_buffer_size) == true) 
                    {
                        ret = 1;
                        break;
                    }
                }
                else
                {
                    if (contains((const char*)_serialBuffer, tag, current_buffer_size) > 0)
                    {
                        ret = 1;
                        break;
                    }
                }
                //_delay_ms(1);
            }
            else 
            {
                for(int i = 0; i < NUMESPTAGS; i++)
                {
                    if (endsWith((const char*)_serialBuffer, ESPTAGS[i], current_buffer_size) == true)
                    {
                        ret = i;
                        break;
                    }
                }
            }
        }
        currtime++;
        _delay_ms(1);
    }

    return ret;
}

void EspConn::reset() {
    //_ringBuffer.reset();
}

}