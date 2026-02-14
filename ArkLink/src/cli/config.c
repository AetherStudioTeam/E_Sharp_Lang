#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



typedef struct {
    const char* data;
    size_t size;
    size_t pos;
} JsonParser;

static void json_skip_whitespace(JsonParser* p) {
    while (p->pos < p->size) {
        char c = p->data[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else if (c == '/' && p->pos + 1 < p->size) {
            
            if (p->data[p->pos + 1] == '/') {
                p->pos += 2;
                while (p->pos < p->size && p->data[p->pos] != '\n') {
                    p->pos++;
                }
            } else if (p->data[p->pos + 1] == '*') {
                p->pos += 2;
                while (p->pos + 1 < p->size && 
                       !(p->data[p->pos] == '*' && p->data[p->pos + 1] == '/')) {
                    p->pos++;
                }
                p->pos += 2;
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

static int json_expect(JsonParser* p, char c) {
    json_skip_whitespace(p);
    if (p->pos >= p->size || p->data[p->pos] != c) {
        return -1;
    }
    p->pos++;
    return 0;
}

static char* json_parse_string(JsonParser* p) {
    json_skip_whitespace(p);
    if (p->pos >= p->size || p->data[p->pos] != '"') {
        return NULL;
    }
    p->pos++; 
    
    size_t start = p->pos;
    while (p->pos < p->size && p->data[p->pos] != '"') {
        if (p->data[p->pos] == '\\' && p->pos + 1 < p->size) {
            p->pos += 2;
        } else {
            p->pos++;
        }
    }
    
    size_t len = p->pos - start;
    char* str = (char*)malloc(len + 1);
    if (!str) {
        return NULL;
    }
    
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (p->data[start + i] == '\\' && start + i + 1 < p->pos) {
            char c = p->data[start + i + 1];
            switch (c) {
            case '"': str[j++] = '"'; break;
            case '\\': str[j++] = '\\'; break;
            case '/': str[j++] = '/'; break;
            case 'b': str[j++] = '\b'; break;
            case 'f': str[j++] = '\f'; break;
            case 'n': str[j++] = '\n'; break;
            case 'r': str[j++] = '\r'; break;
            case 't': str[j++] = '\t'; break;
            default: str[j++] = c; break;
            }
            i++;
        } else {
            str[j++] = p->data[start + i];
        }
    }
    str[j] = '\0';
    
    if (p->pos < p->size && p->data[p->pos] == '"') {
        p->pos++; 
    }
    
    return str;
}

static int json_parse_import_entry(JsonParser* p, ArkImportConfig* import) {
    if (json_expect(p, '{') != 0) {
        return -1;
    }
    
    import->module = NULL;
    import->symbol = NULL;
    import->slot = 0;
    
    while (1) {
        json_skip_whitespace(p);
        if (p->pos < p->size && p->data[p->pos] == '}') {
            p->pos++;
            break;
        }
        
        char* key = json_parse_string(p);
        if (!key) {
            return -1;
        }
        
        if (json_expect(p, ':') != 0) {
            free(key);
            return -1;
        }
        
        if (strcmp(key, "module") == 0) {
            import->module = json_parse_string(p);
        } else if (strcmp(key, "symbol") == 0) {
            import->symbol = json_parse_string(p);
        } else if (strcmp(key, "slot") == 0) {
            json_skip_whitespace(p);
            import->slot = 0;
            while (p->pos < p->size && isdigit((unsigned char)p->data[p->pos])) {
                import->slot = import->slot * 10 + (p->data[p->pos] - '0');
                p->pos++;
            }
        } else {
            
            json_skip_whitespace(p);
            if (p->data[p->pos] == '"') {
                char* dummy = json_parse_string(p);
                free(dummy);
            } else {
                while (p->pos < p->size && p->data[p->pos] != ',' && p->data[p->pos] != '}') {
                    p->pos++;
                }
            }
        }
        
        free(key);
        
        json_skip_whitespace(p);
        if (p->pos < p->size && p->data[p->pos] == ',') {
            p->pos++;
        }
    }
    
    return 0;
}

static int json_parse_imports_array(JsonParser* p, ArkRuntimeConfigParsed* config) {
    if (json_expect(p, '[') != 0) {
        return -1;
    }
    
    config->imports_capacity = 16;
    config->imports = (ArkImportConfig*)calloc(config->imports_capacity, sizeof(ArkImportConfig));
    if (!config->imports) {
        return -1;
    }
    
    json_skip_whitespace(p);
    if (p->pos < p->size && p->data[p->pos] == ']') {
        p->pos++;
        return 0;
    }
    
    while (1) {
        if (config->imports_count >= config->imports_capacity) {
            config->imports_capacity *= 2;
            ArkImportConfig* new_imports = (ArkImportConfig*)realloc(config->imports,
                config->imports_capacity * sizeof(ArkImportConfig));
            if (!new_imports) {
                return -1;
            }
            config->imports = new_imports;
        }
        
        if (json_parse_import_entry(p, &config->imports[config->imports_count]) != 0) {
            return -1;
        }
        config->imports_count++;
        
        json_skip_whitespace(p);
        if (p->pos < p->size && p->data[p->pos] == ',') {
            p->pos++;
        } else if (p->pos < p->size && p->data[p->pos] == ']') {
            p->pos++;
            break;
        } else {
            return -1;
        }
    }
    
    return 0;
}

static int json_parse_object(JsonParser* p, ArkRuntimeConfigParsed* config) {
    if (json_expect(p, '{') != 0) {
        return -1;
    }
    
    while (1) {
        json_skip_whitespace(p);
        if (p->pos < p->size && p->data[p->pos] == '}') {
            p->pos++;
            break;
        }
        
        char* key = json_parse_string(p);
        if (!key) {
            return -1;
        }
        
        if (json_expect(p, ':') != 0) {
            free(key);
            return -1;
        }
        
        if (strcmp(key, "runtime") == 0) {
            config->runtime_name = json_parse_string(p);
        } else if (strcmp(key, "subsystem") == 0) {
            char* subsystem = json_parse_string(p);
            if (subsystem) {
                if (strcmp(subsystem, "console") == 0) {
                    config->subsystem = ARK_SUBSYSTEM_CONSOLE;
                } else if (strcmp(subsystem, "windows") == 0) {
                    config->subsystem = ARK_SUBSYSTEM_WINDOWS;
                }
                free(subsystem);
            }
        } else if (strcmp(key, "imports") == 0) {
            if (json_parse_imports_array(p, config) != 0) {
                free(key);
                return -1;
            }
        } else if (strcmp(key, "entry") == 0) {
            config->entry_point = json_parse_string(p);
        } else if (strcmp(key, "imageBase") == 0) {
            json_skip_whitespace(p);
            if (p->pos < p->size && p->data[p->pos] == '"') {
                char* base_str = json_parse_string(p);
                if (base_str) {
                    config->image_base = strtoull(base_str, NULL, 0);
                    free(base_str);
                }
            } else {
                config->image_base = 0;
                while (p->pos < p->size && isdigit((unsigned char)p->data[p->pos])) {
                    config->image_base = config->image_base * 10 + (p->data[p->pos] - '0');
                    p->pos++;
                }
            }
        } else if (strcmp(key, "stackSize") == 0) {
            json_skip_whitespace(p);
            config->stack_size = 0;
            while (p->pos < p->size && isdigit((unsigned char)p->data[p->pos])) {
                config->stack_size = config->stack_size * 10 + (p->data[p->pos] - '0');
                p->pos++;
            }
        } else {
            
            json_skip_whitespace(p);
            if (p->data[p->pos] == '"') {
                char* dummy = json_parse_string(p);
                free(dummy);
            } else if (p->data[p->pos] == '[') {
                int depth = 1;
                p->pos++;
                while (p->pos < p->size && depth > 0) {
                    if (p->data[p->pos] == '[') depth++;
                    else if (p->data[p->pos] == ']') depth--;
                    p->pos++;
                }
            } else if (p->data[p->pos] == '{') {
                int depth = 1;
                p->pos++;
                while (p->pos < p->size && depth > 0) {
                    if (p->data[p->pos] == '{') depth++;
                    else if (p->data[p->pos] == '}') depth--;
                    p->pos++;
                }
            } else {
                while (p->pos < p->size && p->data[p->pos] != ',' && p->data[p->pos] != '}') {
                    p->pos++;
                }
            }
        }
        
        free(key);
        
        json_skip_whitespace(p);
        if (p->pos < p->size && p->data[p->pos] == ',') {
            p->pos++;
        }
    }
    
    return 0;
}

ArkRuntimeConfigParsed* ark_config_parse_json(const char* json_str) {
    if (!json_str) {
        return NULL;
    }
    
    ArkRuntimeConfigParsed* config = (ArkRuntimeConfigParsed*)calloc(1, sizeof(ArkRuntimeConfigParsed));
    if (!config) {
        return NULL;
    }
    
    
    config->subsystem = ARK_SUBSYSTEM_CONSOLE;
    config->image_base = 0x140000000ULL;
    config->stack_size = 0x100000;
    
    JsonParser parser = {
        .data = json_str,
        .size = strlen(json_str),
        .pos = 0
    };
    
    if (json_parse_object(&parser, config) != 0) {
        ark_config_free(config);
        return NULL;
    }
    
    return config;
}

ArkRuntimeConfigParsed* ark_config_load_file(const char* path) {
    if (!path) {
        return NULL;
    }
    
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    
    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    
    if (fread(content, 1, (size_t)size, fp) != (size_t)size) {
        free(content);
        fclose(fp);
        return NULL;
    }
    content[size] = '\0';
    fclose(fp);
    
    ArkRuntimeConfigParsed* config = ark_config_parse_json(content);
    free(content);
    
    return config;
}

void ark_config_free(ArkRuntimeConfigParsed* config) {
    if (!config) {
        return;
    }
    
    free(config->runtime_name);
    free(config->entry_point);
    
    if (config->imports) {
        for (size_t i = 0; i < config->imports_count; i++) {
            free(config->imports[i].module);
            free(config->imports[i].symbol);
        }
        free(config->imports);
    }
    
    free(config);
}
