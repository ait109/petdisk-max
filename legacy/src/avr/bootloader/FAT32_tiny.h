/*
    FAT32.h
    FAT32 filesystem Routines in the PETdisk storage device
    Copyright (C) 2011 Michael Hill

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    Contact the author at bitfixer@bitfixer.com
    http://bitfixer.com 
*/

#ifndef _FAT32_H_
#define _FAT32_H_

#include "SD_routines.h"
#include "SerialLogger.h"
#include "DataSource.h"

//Structure to access Directory Entry in the FAT
struct dir_Structure{
unsigned char name[11];     //0
unsigned char attrib;       //11 //file attributes
unsigned char NTreserved;   //12 //always 0
unsigned char timeTenth;    //13 //tenths of seconds, set to 0 here
unsigned int createTime;    //14 //time file was created
unsigned int createDate;    //16 //date file was created
unsigned int lastAccessDate;//18
unsigned int firstClusterHI;//20 //higher word of the first cluster number
unsigned int writeTime;     //22 //time of last write
unsigned int writeDate;     //24 //date of last write
unsigned int firstClusterLO;//26 //lower word of the first cluster number
unsigned long fileSize;     //28 //size of file in bytes
    //32
};

// structure for file read information
typedef struct _file_stat{
    unsigned long currentCluster;
    unsigned long fileSize;
    unsigned long currentSector;
    unsigned long byteCounter;
    int sectorIndex;
} file_stat;

typedef struct _file_position {
    unsigned char isLongFilename;
    unsigned char *fileName;
    unsigned long startCluster;
    unsigned long cluster;
    unsigned long dirStartCluster;
    unsigned char sectorIndex;
    unsigned long sector;
    unsigned long fileSize;
    unsigned long byteCounter;
    unsigned int byte;
    unsigned char shortFilename[11];
} file_position;

#define MAX_FILENAME       256

class FAT32
{
public:
    FAT32(SD* sd, unsigned char* fatbuffer)
    : _sd(sd)
    , _FatBuffer(fatbuffer)
    , _currentDirectoryEntry(0)
    {

    }

    ~FAT32() {}

    bool init();
    
    void openDirectory();
    bool getNextDirectoryEntry();
    bool openFileForReading();
    unsigned int getNextFileBlock();
    bool isLastBlock();
    unsigned char* getShortFilename();

private:
    SD* _sd;
    unsigned char* _FatBuffer;
    file_position _filePosition;
    struct dir_Structure* _currentDirectoryEntry;

    unsigned long _firstDataSector;
    unsigned long _rootCluster;
    unsigned long _totalClusters;
    unsigned int _bytesPerSector;
    unsigned int _sectorPerCluster;
    unsigned int _reservedSectorCount;
    unsigned long _unusedSectors;
    unsigned long _appendFileSector;
    unsigned long _appendFileLocation;
    unsigned long _fileSize;
    unsigned long _appendStartCluster;

    unsigned long _fileStartCluster;
    unsigned long _currentDirectoryCluster;

    unsigned char getBootSectorData (void);
    unsigned long getFirstSector(unsigned long clusterNumber);
    
    unsigned long getNextCluster (unsigned long clusterNumber ,unsigned long clusterEntry);
    unsigned long getFirstCluster(struct dir_Structure *dir);
};

#endif
