#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROOT_ADDRESS 0x4c200
#define N 255

typedef struct {
    unsigned char first_byte;
    unsigned char start_chs[3];
    unsigned char partition_type;
    unsigned char end_chs[3];
    unsigned int start_sector;
    unsigned int length_sectors;
} __attribute((packed)) PartitionTable;

typedef struct {
    unsigned char jmp[3];
    char oem[8];
    unsigned short sector_size;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char number_of_fats;
    unsigned short root_dir_entries;
    unsigned short total_sectors_short; // if zero, later field is used
    unsigned char media_descriptor;
    unsigned short fat_size_sectors;
    unsigned short sectors_per_track;
    unsigned short number_of_heads;
    unsigned int hidden_sectors;
    unsigned int total_sectors_long;
    
    unsigned char drive_number;
    unsigned char current_head;
    unsigned char boot_signature;
    unsigned int volume_id;
    char volume_label[11];
    char fs_type[8];
    char boot_code[448];
    unsigned short boot_sector_signature;
} __attribute((packed)) Fat16BootSector;

typedef struct {
    unsigned char filename[8];  
    unsigned char ext[3];
    unsigned char attributes;
    unsigned char reserved[10];
    unsigned short modify_time;
    unsigned short modify_date;
    unsigned short starting_cluster;
    unsigned int file_size;
} __attribute((packed)) Fat16Entry;

void print_file_info(Fat16Entry *entry) {
    switch(entry->filename[0]) {
    case 0x00:
        return; // unused entry
    case 0xE5:
        printf("Deleted file: [?%.7s.%.3s]\n", entry->filename+1, entry->ext);
        return;
    case 0x05:
        printf("File starting with 0xE5: [%c%.7s.%.3s]\n", 0xE5, entry->filename+1, entry->ext);
        break;
    case 0x2E:
        printf("Directory: [%.8s.%.3s]\n", entry->filename, entry->ext);
        break;
    default:
        if(entry->attributes == 16)
            printf("Directory: [%.8s.%.3s]\n", entry->filename, entry->ext);
        else
            printf("File: [%.8s.%.3s]\n", entry->filename, entry->ext);
    }

    printf("  Modified: %04d-%02d-%02d %02d:%02d.%02d    Start: [%04X]    Size: %d\n", 
        1980 + (entry->modify_date >> 9), (entry->modify_date >> 5) & 0xF, entry->modify_date & 0x1F,
        (entry->modify_time >> 11), (entry->modify_time >> 5) & 0x3F, entry->modify_time & 0x1F,
        entry->starting_cluster, entry->file_size);
}

int main() {
    FILE * in = fopen("test.img", "rb");
    int i;
    PartitionTable pt[4];
    Fat16BootSector bs;
    Fat16Entry entry;

    char imageBuffer[1048576];
    fseek(in, 0, SEEK_SET);
    fread(imageBuffer, sizeof(imageBuffer), 1, in);


    fseek(in, 0x1BE, SEEK_SET); // go to partition table start
    fread(pt, sizeof(PartitionTable), 4, in); // read all four entries
    
    for(i=0; i<4; i++) {        
        if(pt[i].partition_type == 4 || pt[i].partition_type == 6 ||
           pt[i].partition_type == 14) {
            //printf("FAT16 filesystem found from partition %d\n", i);
            break;
        }
    }
    
    if(i == 4) {
        printf("No FAT16 filesystem found, exiting...\n");
        return -1;
    }
    
    fseek(in, 512 * pt[i].start_sector, SEEK_SET); //Seek bootsector
    fread(&bs, sizeof(Fat16BootSector), 1, in); //Read bootsector
    
    //Start console
    char path[N] = "\\";
    char folderName[8] = "SUBDIR ";
    int currDirectory = 0x13; //Root

    printf("C:%s>", path);
    gets(path);

    //strcpy(folderName, strtok(path, "\\"))
    /*printf("%s\n", path);
    printf("Searching for %s in %d\n", folderName, currDirectory);*/

    //Loop start
    //do {
        if(currDirectory == 0){
            fseek(in, ROOT_ADDRESS, SEEK_SET); //Seek for folder
            do {
                fread(&entry, sizeof(entry), 1, in);
                char fileName[8];
                for(int j = 0; j < 8; j++){
                    fileName[j] = entry.filename[j];
                }

                /*printf("%d %d %d %d %d %d %d %d \n",fileName[0],fileName[1],fileName[2],fileName[3],fileName[4],fileName[5], fileName[6], fileName[7]);
                printf("%d %d %d %d %d %d %d %d \n",folderName[0],folderName[1],folderName[2],folderName[3],folderName[4],folderName[5], folderName[6], folderName[7]);
                printf("Comp %d\n", strcmp(folderName, fileName));
                //printf("%s_", sizeof(entry.filename));
                //printf("%d-\n", strcmp(folderName, entry.filename));*/
                if ((strcmp(folderName, fileName) < 0) && entry.attributes == 16){
                    currDirectory = entry.starting_cluster;
                    
                    break;
                }
            } while(entry.filename[0] != 0x00);
        } else {
            fseek(in, (ROOT_ADDRESS + (0x4000 * (currDirectory - 1))), SEEK_SET); //Seek for folder
            do {
                fread(&entry, sizeof(entry), 1, in);
                char fileName[8];
                for(int j = 0; j < 8; j++){
                    fileName[j] = entry.filename[j];
                }
                //printf("%s_", sizeof(entry.filename));
                printf("%d-\n", strcmp(folderName, entry.filename));
                if ((strcmp(folderName, fileName) > 0) && entry.attributes == 16){
                    currDirectory = entry.starting_cluster;
                    break;
                }
            } while(entry.filename[0] != 0x00);
        }*/
    //} while(strcpy(folderName, strtok(NULL, "\\")));
    //Loop end

    //printf("Now in %d\n", currDirectory);
    //TODO: For ls -l
    /*if(currDirectory == 0){
        fseek(in, ROOT_ADDRESS, SEEK_SET); //Seek for folder
        for(i=0; i<bs.root_dir_entries; i++) {
            fread(&entry, sizeof(entry), 1, in);
            print_file_info(&entry);
        }
    } else {
        fseek(in, (ROOT_ADDRESS + (0x4000 * (currDirectory - 1))), SEEK_SET); //Seek for folder
        do {
            fread(&entry, sizeof(entry), 1, in);
            print_file_info(&entry);
        } while(entry.filename[0] != 0x00);
    }


//TODO:mkdir [NAME]
    /*Fat16Entry newDirEntry;
    strcpy(newDirEntry.filename, "NEWDIR");
    strcpy(newDirEntry.ext, "   ");
    newDirEntry.attributes = 16;
    newDirEntry.file_size = 0;

    //Search free sector
    int newDirOff = 1;
    do { //Skip to free sector
        newDirOff++;
        fseek(in, (ROOT_ADDRESS + (0x4000 * (newDirOff - 1))), SEEK_SET); //Seek for folder
        fread(&entry, sizeof(entry), 1, in);

    } while(entry.filename[0] != 0x00);
    printf("Free sector found in %d\n", newDirOff);
    newDirEntry.starting_cluster = newDirOff;
    
    //Writing dir entry
    if(currDirectory == 0){
        fseek(in, ROOT_ADDRESS, SEEK_SET); //Seek for folder
        do {
            fread(&entry, sizeof(entry), 1, in);
        } while(entry.filename[0] != 0x00);
        //Write here
        int pointer = ftell(in) - 32;
        
        memcpy(&(imageBuffer[pointer]), &newDirEntry, sizeof(newDirEntry));
    } else {
        fseek(in, (ROOT_ADDRESS + (0x4000 * (currDirectory - 1))), SEEK_SET); //Seek for folder

        do {
            fread(&entry, sizeof(entry), 1, in);
        } while(entry.filename[0] != 0x00);
        //Write here
        int pointer = ftell(in) - 32;
        
        memcpy(&(imageBuffer[pointer]), &newDirEntry, sizeof(newDirEntry));
    }

    //Write folder on image
    Fat16Entry currentDirEntry; //Dir .
    Fat16Entry parentDirEntry; //Dir ..

    //Dir . entry
    strcpy(currentDirEntry.filename, ".       ");
    strcpy(currentDirEntry.ext, "   ");
    currentDirEntry.attributes = 16;
    currentDirEntry.starting_cluster = newDirOff;
    currentDirEntry.file_size = 0;

    //Dir .. entry
    strcpy(parentDirEntry.filename, "..      ");
    strcpy(parentDirEntry.ext, "   ");
    parentDirEntry.attributes = 16;
    parentDirEntry.starting_cluster = currDirectory;
    parentDirEntry.file_size = 0;

    
    //Write to buffer
    int newDirpt = (ROOT_ADDRESS + (0x4000 * (newDirOff - 1)));
    memcpy(&(imageBuffer[newDirpt]), &currentDirEntry, sizeof(currentDirEntry));
    newDirpt += 32;
    memcpy(&(imageBuffer[newDirpt]), &parentDirEntry, sizeof(parentDirEntry));

    //Write to file
    fclose(in);
    FILE * out = fopen("test.img", "wb");
    fwrite(imageBuffer , 1, sizeof(imageBuffer), out);
    fclose(out);
    in = fopen("test.img", "rb");
    fclose(in);
    //End Write to file*/
//End mkdir

//cat > [NAME.EXT]
    /*Fat16Entry newDirEntry;
    strcpy(newDirEntry.filename, "A      ");
    strcpy(newDirEntry.ext, "TXT");
    newDirEntry.attributes = 32;
    newDirEntry.file_size = 4;

    //Search free sector
    int newDirOff = 1;
    do { //Skip to free sector
        newDirOff++;
        fseek(in, (ROOT_ADDRESS + (0x4000 * (newDirOff - 1))), SEEK_SET); //Seek for folder
        fread(&entry, sizeof(entry), 1, in);

    } while(entry.filename[0] != 0x00);
    printf("Free cluster found in %d\n", newDirOff);
    newDirEntry.starting_cluster = newDirOff;
    
    //Writing dir entry
    if(currDirectory == 0){
        fseek(in, ROOT_ADDRESS, SEEK_SET); //Seek for folder
        do {
            fread(&entry, sizeof(entry), 1, in);
        } while(entry.filename[0] != 0x00);
        //Write here
        int pointer = ftell(in) - 32;
        
        memcpy(&(imageBuffer[pointer]), &newDirEntry, sizeof(newDirEntry));
    } else {
        fseek(in, (ROOT_ADDRESS + (0x4000 * (currDirectory - 1))), SEEK_SET); //Seek for folder

        do {
            fread(&entry, sizeof(entry), 1, in);
        } while(entry.filename[0] != 0x00);
        //Write here
        int pointer = ftell(in) - 32;
        
        memcpy(&(imageBuffer[pointer]), &newDirEntry, sizeof(newDirEntry));
    }

    //Write to buffer
    int newDirpt = (ROOT_ADDRESS + (0x4000 * (newDirOff - 1)));
    char hola[4] = "Hola";
    memcpy(&(imageBuffer[newDirpt]), &hola, sizeof(hola));

    //Write to file
    fclose(in);
    FILE * out = fopen("test.img", "wb");
    fwrite(imageBuffer , 1, sizeof(imageBuffer), out);
    fclose(out);
    in = fopen("test.img", "rb");
    fclose(in);*/
    //End Write to file
//End cat > a.txt

    fclose(in);
    return 0;
}