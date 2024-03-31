#include "Settings.h"
#include <string.h>
#include <stdlib.h>
#include "hardware.h"

void Settings::initWithParams(void* eepromHost, int eepromHostLength, int port, void* eepromUrl, int eepromUrlLength)
{
    _urlData.eepromHost = eepromHost;
    _urlData.eepromHostLength = eepromHostLength;
    _urlData.port = port;
    _urlData.eepromUrl = eepromUrl;
    _urlData.eepromUrlLength = eepromUrlLength;
}

int Settings::getUrl(char* url)
{
    bf_eeprom_read_block(url, _urlData.eepromUrl, _urlData.eepromUrlLength);
    url[_urlData.eepromUrlLength] = 0;
    return _urlData.eepromUrlLength;
}

int Settings::getHost(char* host)
{
   bf_eeprom_read_block(host, _urlData.eepromHost, _urlData.eepromHostLength);
   host[_urlData.eepromHostLength] = 0;
   return _urlData.eepromHostLength;
}

int Settings::getPort()
{
    return _urlData.port;
}