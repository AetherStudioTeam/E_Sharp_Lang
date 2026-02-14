#include "archive.h"
#include <stdlib.h>
#include <string.h>


#define AR_ARCHIVE_MAGIC "!<arch>\n"
#define AR_ARCHIVE_MAGIC_LEN 8


#define COFF_IMPORT_SIG "/               "
#define COFF_IMPORT_SIG_LEN 16


#define THIN_ARCHIVE_MAGIC "!<thin>\n"
#define THIN_ARCHIVE_MAGIC_LEN 8


typedef struct {
    char name[16];        
    char date[12];        
    char uid[6];          
    char gid[6];          
    char mode[8];         
    char size[10];        
    char end[2];          
} ArArchiveHeader;


static int parse_decimal(const char* str, size_t len) {
    int result = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            result = result * 10 + (str[i] - '0');
        } else if (str[i] != ' ') {
            break;
        }
    }
    return result;
}

static ArkArchiveResult parse_ar_archive(ArkArchive* archive, FILE* fp) {
    
    if (fseek(fp, AR_ARCHIVE_MAGIC_LEN, SEEK_SET) != 0) {
        return ARK_ARCHIVE_ERR_IO;
    }

    
    ArArchiveHeader first_hdr;
    if (fread(&first_hdr, sizeof(first_hdr), 1, fp) != 1) {
        return ARK_ARCHIVE_ERR_FORMAT;
    }

    
    if (strncmp(first_hdr.name, COFF_IMPORT_SIG, COFF_IMPORT_SIG_LEN) == 0) {
        archive->type = ARK_ARCHIVE_COFF_IMPORT;
    } else {
        
        archive->type = ARK_ARCHIVE_UNKNOWN;
    }

    
    long current_offset = AR_ARCHIVE_MAGIC_LEN;
    
    while (1) {
        
        ArArchiveHeader hdr;
        if (fseek(fp, current_offset, SEEK_SET) != 0) {
            break;
        }
        
        if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
            break;
        }

        
        if (hdr.end[0] != '`' || hdr.end[1] != '\n') {
            
            break;
        }

        
        int size = parse_decimal(hdr.size, 10);
        if (size <= 0) {
            break;
        }

        
        
        bool is_special = (hdr.name[0] == '/' || 
                          (hdr.name[0] == '/' && hdr.name[1] == '/') ||
                          strncmp(hdr.name, "__.SYMDEF", 9) == 0);

        if (!is_special) {
            
            if (archive->object_count >= archive->object_capacity) {
                archive->object_capacity = archive->object_capacity ? 
                    archive->object_capacity * 2 : 16;
                ArkArchiveObject* new_objects = realloc(archive->objects, 
                    archive->object_capacity * sizeof(ArkArchiveObject));
                if (!new_objects) {
                    return ARK_ARCHIVE_ERR_MEMORY;
                }
                archive->objects = new_objects;
            }

            ArkArchiveObject* obj = &archive->objects[archive->object_count++];
            
            
            size_t name_len = 16;
            while (name_len > 0 && hdr.name[name_len - 1] == ' ') {
                name_len--;
            }
            if (name_len > 0 && hdr.name[name_len - 1] == '/') {
                name_len--;
            }
            
            obj->name = malloc(name_len + 1);
            if (!obj->name) {
                return ARK_ARCHIVE_ERR_MEMORY;
            }
            memcpy(obj->name, hdr.name, name_len);
            obj->name[name_len] = '\0';

            
            obj->size = size;
            obj->data = malloc(size);
            if (!obj->data) {
                free(obj->name);
                return ARK_ARCHIVE_ERR_MEMORY;
            }

            long data_offset = current_offset + sizeof(ArArchiveHeader);
            if (fseek(fp, data_offset, SEEK_SET) != 0 ||
                fread(obj->data, 1, size, fp) != size) {
                free(obj->name);
                free(obj->data);
                return ARK_ARCHIVE_ERR_IO;
            }

            obj->offset = data_offset;
        }

        
        current_offset += sizeof(ArArchiveHeader) + size;
        if (size % 2 != 0) {
            current_offset++; 
        }
    }

    return ARK_ARCHIVE_OK;
}

ArkArchiveResult ark_archive_open(const char* filename, ArkArchive** archive) {
    if (!filename || !archive) {
        return ARK_ARCHIVE_ERR_FORMAT;
    }

    *archive = NULL;

    
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return ARK_ARCHIVE_ERR_IO;
    }

    
    char magic[AR_ARCHIVE_MAGIC_LEN];
    if (fread(magic, 1, AR_ARCHIVE_MAGIC_LEN, fp) != AR_ARCHIVE_MAGIC_LEN) {
        fclose(fp);
        return ARK_ARCHIVE_ERR_FORMAT;
    }

    
    if (memcmp(magic, AR_ARCHIVE_MAGIC, AR_ARCHIVE_MAGIC_LEN) != 0 &&
        memcmp(magic, THIN_ARCHIVE_MAGIC, THIN_ARCHIVE_MAGIC_LEN) != 0) {
        fclose(fp);
        return ARK_ARCHIVE_ERR_FORMAT;
    }

    
    ArkArchive* ar = calloc(1, sizeof(ArkArchive));
    if (!ar) {
        fclose(fp);
        return ARK_ARCHIVE_ERR_MEMORY;
    }

    ar->filename = strdup(filename);
    if (!ar->filename) {
        free(ar);
        fclose(fp);
        return ARK_ARCHIVE_ERR_MEMORY;
    }

    
    ArkArchiveResult result = parse_ar_archive(ar, fp);
    fclose(fp);

    if (result != ARK_ARCHIVE_OK) {
        ark_archive_close(ar);
        return result;
    }

    *archive = ar;
    return ARK_ARCHIVE_OK;
}

void ark_archive_close(ArkArchive* archive) {
    if (!archive) {
        return;
    }

    if (archive->objects) {
        for (size_t i = 0; i < archive->object_count; i++) {
            free(archive->objects[i].name);
            free(archive->objects[i].data);
        }
        free(archive->objects);
    }

    free(archive->filename);
    free(archive);
}

ArkArchiveType ark_archive_detect_type(const char* filename) {
    
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return ARK_ARCHIVE_UNKNOWN;
    }

    if (strcmp(ext, ".lib") == 0) {
        return ARK_ARCHIVE_COFF_STATIC; 
    } else if (strcmp(ext, ".a") == 0) {
        return ARK_ARCHIVE_ELF_STATIC; 
    }

    return ARK_ARCHIVE_UNKNOWN;
}

bool ark_archive_is_archive(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return false;
    }

    char magic[AR_ARCHIVE_MAGIC_LEN];
    size_t read = fread(magic, 1, AR_ARCHIVE_MAGIC_LEN, fp);
    fclose(fp);

    if (read != AR_ARCHIVE_MAGIC_LEN) {
        return false;
    }

    return (memcmp(magic, AR_ARCHIVE_MAGIC, AR_ARCHIVE_MAGIC_LEN) == 0 ||
            memcmp(magic, THIN_ARCHIVE_MAGIC, THIN_ARCHIVE_MAGIC_LEN) == 0);
}
