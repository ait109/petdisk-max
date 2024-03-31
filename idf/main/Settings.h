#ifndef __settings_h__
#define __settings_h__

struct urlData
{
    void* eepromHost;
    int eepromHostLength;
    int port;
    void* eepromUrl;
    int eepromUrlLength;
};

class Settings {
public:
    Settings() {};
    ~Settings() {};

    void initWithParams(void* eepromHost, int eepromHostLength, int port, void* eepromUrl, int eepromUrlLength);
    int getUrl(char* url);
    int getHost(char* host);
    int getPort();

private:
    urlData _urlData;
};

#endif