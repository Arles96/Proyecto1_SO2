#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

using namespace std;

// variables globales

const int textSize = 255;

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

void ls_l (Fat16Entry *entry) {
  switch(entry->filename[0]) {
    case 0x00:
        return; // unused entry
    case 0xE5:
        return; // don't show
    case 0x05:
        break; // don't show
    case 0x2E: {
        printf("directorio\t%04d-%02d-%02d %02d:%02d.%02d\t%d", 1980 + (entry->modify_date >> 9),
          (entry->modify_date >> 5) & 0xF, entry->modify_date & 0x1F, (entry->modify_time >> 11),
          (entry->modify_time >> 5) & 0x3F, entry->modify_time & 0x1F, entry->file_size);
        printf("\t%.8s.%.3s\n", entry->filename, entry->ext);
        break;
    }
    default: {
      if (entry->file_size == 0) {
        printf("directorio\t%04d-%02d-%02d %02d:%02d.%02d\t%d", 1980 + (entry->modify_date >> 9),
          (entry->modify_date >> 5) & 0xF, entry->modify_date & 0x1F, (entry->modify_time >> 11),
          (entry->modify_time >> 5) & 0x3F, entry->modify_time & 0x1F, entry->file_size);
        printf("\t%.8s.%.3s\n", entry->filename, entry->ext);
      } else {
        printf("archivo\t%04d-%02d-%02d %02d:%02d.%02d\t%d", 1980 + (entry->modify_date >> 9),
        (entry->modify_date >> 5) & 0xF, entry->modify_date & 0x1F, (entry->modify_time >> 11),
        (entry->modify_time >> 5) & 0x3F, entry->modify_time & 0x1F, entry->file_size);
        printf("\t%.8s.%.3s\n", entry->filename, entry->ext);
      }
    }
  }
}

void read_file(FILE *in, unsigned int fat_start, unsigned int data_start,
  unsigned int cluster_size, unsigned short cluster, unsigned int file_size) {
    unsigned char buffer[4096];
    size_t bytes_read, bytes_to_read,
           file_left = file_size, cluster_left = cluster_size;

    // Go to first data cluster
    fseek(in, data_start + cluster_size * (cluster-2), SEEK_SET);

    // Read until we run out of file or clusters
    while(file_left > 0 && cluster != 0xFFFF) {
      bytes_to_read = sizeof(buffer);

      // don't read past the file or cluster end
      if(bytes_to_read > file_left)
          bytes_to_read = file_left;
      if(bytes_to_read > cluster_left)
          bytes_to_read = cluster_left;

      // read data from cluster, write to file
      bytes_read = fread(buffer, 1, bytes_to_read, in);
      // print file data
      cout << buffer << endl;

      // decrease byte counters for current cluster and whole file
      cluster_left -= bytes_read;
      file_left -= bytes_read;

      // if we have read the whole cluster, read next cluster # from FAT
      if(cluster_left == 0) {
        fseek(in, fat_start + cluster*2, SEEK_SET);
        fread(&cluster, 2, 1, in);

        printf("End of cluster reached, next cluster %d\n", cluster);

        fseek(in, data_start + cluster_size * (cluster-2), SEEK_SET);
        cluster_left = cluster_size; // reset cluster byte counter
      }
    }
}

bool isCatRead (string command) {
  if (command.size() >= 4) {
    string cat = command.substr(0, 4);
    if (cat == "cat ") {
      string file = command.substr(4, command.size());
      cout << file << endl;
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

void catRead (FILE *in, PartitionTable *pt, Fat16BootSector bs, Fat16Entry entry, string file) {
  // Copy filename and extension to space-padded search strings
  int i, j;
  unsigned int fat_start, root_start, data_start;
  char filename[9] = "        ", file_ext[4] = "   "; // initially pad with spaces
  for(i=0; i<8 && file[i] != '.' && file[i] != 0; i++)
    filename[i] = file[i];
  for(j=1; j<=3 && file[i+j] != 0; j++)
    file_ext[j-1] = file[i+j];

  // printf("Opened %s, looking for [%s.%s]\n", argv[1], filename, file_ext);

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
        return;
    }

    fseek(in, 512 * pt[i].start_sector, SEEK_SET);
    fread(&bs, sizeof(Fat16BootSector), 1, in);

    // Calculate start offsets of FAT, root directory and data
    fat_start = ftell(in) + (bs.reserved_sectors-1) * bs.sector_size;
    root_start = fat_start + bs.fat_size_sectors * bs.number_of_fats *
        bs.sector_size;
    data_start = root_start + bs.root_dir_entries * sizeof(Fat16Entry);

    /* printf("FAT start at %08X, root dir at %08X, data at %08X\n",
            fat_start, root_start, data_start); */

    fseek(in, root_start, SEEK_SET);

    /* cout << filename << endl;
    cout << file_ext << endl; */

    for(i=0; i<bs.root_dir_entries; i++) {
      fread(&entry, sizeof(entry), 1, in);

      if(memcmp(entry.filename, filename, 8) == 0 &&
          memcmp(entry.ext, file_ext, 3) == 0) {
              // printf("File found!\n");
              break;
          }
    }

    if(i == bs.root_dir_entries) {
        printf("Archivo no encontrado!");
        return;
    }

    // out = fopen(argv[2], "wb"); // write the file contents to disk
    read_file(in, fat_start, data_start, bs.sectors_per_cluster *
                bs.sector_size, entry.starting_cluster, entry.file_size);
}

int main() {
    string command;
    bool stop = false;
    int counter = 0;
    
      FILE * in = fopen("test.img", "rb");
      int i;
      PartitionTable pt[4];
      Fat16BootSector bs;
      Fat16Entry entry;

      fseek(in, 0x1BE, SEEK_SET); // go to partition table start
      fread(pt, sizeof(PartitionTable), 4, in); // read all four entries

      for(i=0; i<4; i++) {
        if(pt[i].partition_type == 4 || pt[i].partition_type == 6 ||
          pt[i].partition_type == 14) {
            if (counter == 0) {
              printf("FAT16 filesystem found from partition %d\n", i);
            }
          break;
        }
      }

      if(i == 4) {
        printf("No FAT16 filesystem found, exiting...\n");
        return -1;
      }

      fseek(in, 512 * pt[i].start_sector, SEEK_SET);
      fread(&bs, sizeof(Fat16BootSector), 1, in);

      /*if (counter == 0) {
        printf("Now at 0x%X, sector size %d, FAT size %d sectors, %d FATs\n\n",
            ftell(in), bs.sector_size, bs.fat_size_sectors, bs.number_of_fats);
      }*/

      fseek(in, (bs.reserved_sectors-1 + bs.fat_size_sectors * bs.number_of_fats) *
            bs.sector_size, SEEK_CUR);
    while (!stop) {
      printf(">");
      getline(cin, command);
      if (command == "ls -l") {
        for(i=0; i<bs.root_dir_entries; i++) {
          fread(&entry, sizeof(entry), 1, in);
          ls_l(&entry);
        }
      } else if (command.substr(0,2) == "cd" ){
        
      } else if (command == "exit") {
        cout << "ha salido de la consola" << endl;
        stop = true;
      } else if (isCatRead(command)) {
        catRead(in, pt, bs, entry, command.substr(4, command.size()));
      }
      fclose(in);
      //counter++;
    }
    // printf("\nRoot directory read, now at 0x%X\n", ftell(in));
    return 0;
}