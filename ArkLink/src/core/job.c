#include "ArkLink/arklink.h"
#include "ArkLink/context.h"
#include "ArkLink/loader.h"
#include "ArkLink/resolver.h"
#include "ArkLink/backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern void ark_backend_register_all(void);

static ArkLinkResult arklink_run_backends(ArkLinkContext* ctx, ArkResolverPlan* plan) {
    if (!plan || !plan->backend_input) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    ArkLinkTarget target = ark_context_job(ctx)->target;
    const ArkBackendOps* ops = ark_backend_query(target);
    if (!ops) {
        return ARK_LINK_ERR_BACKEND;
    }
    ArkBackendState* state = NULL;
    ArkLinkResult res = ops->prepare(ctx, plan->backend_input, &state);
    if (res != ARK_LINK_OK) {
        return res;
    }
    const char* output = ark_context_job(ctx)->output;
    res = ops->write_output(state, output);
    ops->destroy_state(state);
    return res;
}

ArkLinkResult arklink_link(const ArkLinkJob* job) {
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] arklink_link started\n");
    fflush(stderr);
#endif
    
    if (!job || !job->inputs || job->input_count == 0) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] Registering backends...\n");
    fflush(stderr);
#endif
    ark_backend_register_all();
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] Creating context...\n");
    fflush(stderr);
#endif
    
    ArkLinkContext* ctx = ark_context_create(job, NULL);
    if (!ctx) {
#ifdef ARKLINK_DEBUG
        fprintf(stderr, "[DEBUG] Failed to create context\n");
        fflush(stderr);
#endif
        return ARK_LINK_ERR_INTERNAL;
    }
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] Context created\n");
    fflush(stderr);
#endif

    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] Allocating config...\n");
    fflush(stderr);
#endif
    
    ArkParsedConfig* config = (ArkParsedConfig*)calloc(1, sizeof(ArkParsedConfig));
    if (!config) {
#ifdef ARKLINK_DEBUG
        fprintf(stderr, "[DEBUG] Failed to allocate config\n");
        fflush(stderr);
#endif
        ark_context_destroy(ctx);
        return ARK_LINK_ERR_INTERNAL;
    }
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] Config allocated, copying data...\n");
    fflush(stderr);
#endif
    
    if (job->runtime.runtime_name) config->runtime_name = strdup(job->runtime.runtime_name);
    if (job->runtime.entry_point) config->entry_point = strdup(job->runtime.entry_point);
    config->subsystem = job->runtime.subsystem;
    config->image_base = job->runtime.image_base;
    config->stack_size = job->runtime.stack_size;
    config->output_kind = job->output_kind;
    
    
    if (job->runtime.import_count > 0) {
        config->imports = (ArkParsedImport*)calloc(job->runtime.import_count, sizeof(ArkParsedImport));
        if (!config->imports) {
            free(config);
            ark_context_destroy(ctx);
            return ARK_LINK_ERR_INTERNAL;
        }
        config->import_count = job->runtime.import_count;
        for (size_t i = 0; i < job->runtime.import_count; i++) {
            config->imports[i].module = strdup(job->runtime.imports[i].module);
            config->imports[i].symbol = strdup(job->runtime.imports[i].symbol);
            config->imports[i].slot = job->runtime.imports[i].slot;
        }
    }
    
    
    if (job->runtime.export_symbol_count > 0) {
        config->export_symbols = (const char**)calloc(job->runtime.export_symbol_count, sizeof(const char*));
        if (!config->export_symbols) {
            
            
            free(config->imports); 
            free(config);
            ark_context_destroy(ctx);
            return ARK_LINK_ERR_INTERNAL;
        }
        config->export_symbol_count = job->runtime.export_symbol_count;
        for (size_t i = 0; i < job->runtime.export_symbol_count; i++) {
            config->export_symbols[i] = strdup(job->runtime.export_symbols[i]);
        }
    }

    ark_context_set_config(ctx, config);

    ArkLinkResult res = ARK_LINK_OK;
    ArkLinkUnit** units = (ArkLinkUnit**)calloc(job->input_count, sizeof(ArkLinkUnit*));
    if (!units) {
        ark_context_destroy(ctx);
        return ARK_LINK_ERR_INTERNAL;
    }

    for (size_t i = 0; i < job->input_count; ++i) {
        const ArkLinkInput* in = &job->inputs[i];
        ArkLoaderDiagnostics diag = {0};

        if (in->data && in->size > 0) {
            const char* name = in->path ? in->path : "<memory>";
            res = ark_loader_load_unit_memory(ctx, name, in->data, in->size, NULL, &units[i], &diag);
        } else {
            const char* path = in->path;
            if (!path) {
                ark_context_log(ctx, ARK_LOG_ERROR, "Input %zu missing path and data", i);
                res = ARK_LINK_ERR_INVALID_ARGUMENT;
                goto cleanup;
            }

            const char* ext = strrchr(path, '.');
            if (ext && (strcmp(ext, ".a") == 0 || strcmp(ext, ".lib") == 0)) {
                


                ark_context_log(ctx, ARK_LOG_WARN, "Archive %s in input list not yet fully supported as primary input", path);
            }
            res = ark_loader_load_unit(ctx, path, NULL, &units[i], &diag);
        }
        
        if (res != ARK_LINK_OK) {
            const char* label = in->path ? in->path : "<memory>";
            ark_context_log(ctx, ARK_LOG_ERROR, "Failed to load %s", label);
            goto cleanup;
        }
    }

    

    ArkResolverPlan plan = {0};
    res = ark_resolver_resolve(ctx, units, job->input_count, &plan);
    if (res != ARK_LINK_OK) {
        goto cleanup_plan;
    }

    res = arklink_run_backends(ctx, &plan);

cleanup_plan:
    ark_resolver_plan_destroy(ctx, &plan);
cleanup:
    for (size_t i = 0; i < job->input_count; ++i) {
        ark_loader_unit_destroy(ctx, units[i]);
    }
    free(units);
    ark_context_destroy(ctx);
    return res;
}

const char* arklink_error_string(ArkLinkResult code) {
    switch (code) {
    case ARK_LINK_OK:
        return "OK";
    case ARK_LINK_ERR_INVALID_ARGUMENT:
        return "Invalid argument";
    case ARK_LINK_ERR_IO:
        return "I/O error";
    case ARK_LINK_ERR_FORMAT:
        return "Bad input format";
    case ARK_LINK_ERR_UNRESOLVED_SYMBOL:
        return "Unresolved symbol";
    case ARK_LINK_ERR_BACKEND:
        return "Backend error";
    case ARK_LINK_ERR_UNSUPPORTED:
        return "Unsupported feature";
    case ARK_LINK_ERR_INTERNAL:
    default:
        return "Internal error";
    }
}





struct ArkLinkSession {
    ArkLinkJob job;
    ArkLinkInput* inputs;
    size_t input_capacity;
    
    ArkLinkImportEntry* imports;
    size_t import_capacity;
    
    const char** library_paths;
    size_t library_path_capacity;
    
    const char** libraries;
    size_t library_capacity;
    
    const char** export_symbols;
    size_t export_capacity;
    
    ArkLinkResult last_error;
};

ArkLinkSession* arklink_session_create(void) {
    ArkLinkSession* session = (ArkLinkSession*)calloc(1, sizeof(ArkLinkSession));
    if (!session) return NULL;
    
    
    session->job.target = ARK_LINK_TARGET_PE;
    session->job.output_kind = ARK_LINK_OUTPUT_EXECUTABLE;
    session->job.runtime.subsystem = ARK_SUBSYSTEM_CONSOLE;
    session->job.runtime.image_base = 0x140000000ULL;
    session->job.runtime.stack_size = 0x100000;
    
    return session;
}

void arklink_session_destroy(ArkLinkSession* session) {
    if (!session) return;
    
    if (session->inputs) {
        for (size_t i = 0; i < session->job.input_count; i++) {
            
            
        }
        free(session->inputs);
    }
    
    free(session->imports);
    free(session->library_paths);
    free(session->libraries);
    free(session->export_symbols);
    free(session);
}

ArkLinkResult arklink_session_set_target(ArkLinkSession* session, ArkLinkTarget target) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.target = target;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_output(ArkLinkSession* session, const char* output_path) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.output = output_path;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_output_kind(ArkLinkSession* session, ArkLinkOutputKind kind) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.output_kind = kind;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_entry_point(ArkLinkSession* session, const char* entry_point) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.runtime.entry_point = entry_point;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_subsystem(ArkLinkSession* session, ArkSubsystem subsystem) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.runtime.subsystem = subsystem;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_image_base(ArkLinkSession* session, uint64_t image_base) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.runtime.image_base = image_base;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_stack_size(ArkLinkSession* session, uint64_t stack_size) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.runtime.stack_size = stack_size;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_logger(ArkLinkSession* session, ArkLinkLogger logger, void* user_data) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.logger = logger;
    session->job.logger_user_data = user_data;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_flags(ArkLinkSession* session, uint32_t flags) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->job.flags = flags;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_add_input(ArkLinkSession* session, const char* path) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    
    if (session->job.input_count >= session->input_capacity) {
        size_t new_cap = session->input_capacity ? session->input_capacity * 2 : 16;
        ArkLinkInput* new_inputs = (ArkLinkInput*)realloc(session->inputs, new_cap * sizeof(ArkLinkInput));
        if (!new_inputs) return ARK_LINK_ERR_INTERNAL;
        session->inputs = new_inputs;
        session->input_capacity = new_cap;
        session->job.inputs = session->inputs;
    }
    
    ArkLinkInput* in = &session->inputs[session->job.input_count];
    memset(in, 0, sizeof(*in));
    in->path = path;
    in->data = NULL;
    in->size = 0;
    session->job.input_count++;
    
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_add_import(ArkLinkSession* session, const char* module, const char* symbol, uint32_t slot) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    
    if (session->job.runtime.import_count >= session->import_capacity) {
        size_t new_cap = session->import_capacity ? session->import_capacity * 2 : 16;
        ArkLinkImportEntry* new_imports = (ArkLinkImportEntry*)realloc(session->imports, new_cap * sizeof(ArkLinkImportEntry));
        if (!new_imports) return ARK_LINK_ERR_INTERNAL;
        session->imports = new_imports;
        session->import_capacity = new_cap;
        session->job.runtime.imports = session->imports;
    }
    
    session->imports[session->job.runtime.import_count].module = module;
    session->imports[session->job.runtime.import_count].symbol = symbol;
    session->imports[session->job.runtime.import_count].slot = slot;
    session->job.runtime.import_count++;
    
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_add_export(ArkLinkSession* session, const char* symbol) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    
    if (session->job.runtime.export_symbol_count >= session->export_capacity) {
        size_t new_cap = session->export_capacity ? session->export_capacity * 2 : 16;
        const char** new_exports = (const char**)realloc(session->export_symbols, new_cap * sizeof(const char*));
        if (!new_exports) return ARK_LINK_ERR_INTERNAL;
        session->export_symbols = new_exports;
        session->export_capacity = new_cap;
        session->job.runtime.export_symbols = session->export_symbols;
    }
    
    session->export_symbols[session->job.runtime.export_symbol_count++] = symbol;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_link(ArkLinkSession* session) {
    if (!session) return ARK_LINK_ERR_INVALID_ARGUMENT;
    session->last_error = arklink_link(&session->job);
    return session->last_error;
}

const char* arklink_session_get_error(const ArkLinkSession* session) {
    if (!session) return "Invalid session";
    return arklink_error_string(session->last_error);
}


ArkLinkResult arklink_session_add_input_memory(ArkLinkSession* session, const char* name, const void* data, size_t size) {
    if (!session || !name || !data || size == 0) return ARK_LINK_ERR_INVALID_ARGUMENT;

    if (session->job.input_count >= session->input_capacity) {
        size_t new_cap = session->input_capacity ? session->input_capacity * 2 : 16;
        ArkLinkInput* new_inputs = (ArkLinkInput*)realloc(session->inputs, new_cap * sizeof(ArkLinkInput));
        if (!new_inputs) return ARK_LINK_ERR_INTERNAL;
        session->inputs = new_inputs;
        session->input_capacity = new_cap;
        session->job.inputs = session->inputs;
    }

    ArkLinkInput* in = &session->inputs[session->job.input_count];
    in->path = name;
    in->data = data;
    in->size = size;

    session->job.input_count++;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_add_library_path(ArkLinkSession* session, const char* path) {
    if (!session || !path) return ARK_LINK_ERR_INVALID_ARGUMENT;

    if (session->job.library_path_count >= session->library_path_capacity) {
        size_t new_cap = session->library_path_capacity ? session->library_path_capacity * 2 : 8;
        const char** new_paths = (const char**)realloc(session->library_paths, new_cap * sizeof(const char*));
        if (!new_paths) return ARK_LINK_ERR_INTERNAL;
        session->library_paths = new_paths;
        session->library_path_capacity = new_cap;
        session->job.library_paths = session->library_paths;
    }

    session->library_paths[session->job.library_path_count++] = path;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_add_library(ArkLinkSession* session, const char* name) {
    if (!session || !name) return ARK_LINK_ERR_INVALID_ARGUMENT;

    if (session->job.library_count >= session->library_capacity) {
        size_t new_cap = session->library_capacity ? session->library_capacity * 2 : 8;
        const char** new_libs = (const char**)realloc(session->libraries, new_cap * sizeof(const char*));
        if (!new_libs) return ARK_LINK_ERR_INTERNAL;
        session->libraries = new_libs;
        session->library_capacity = new_cap;
        session->job.libraries = session->libraries;
    }

    session->libraries[session->job.library_count++] = name;
    return ARK_LINK_OK;
}



typedef struct {
    const char* data;
    size_t size;
    size_t pos;
} SimpleJsonParser;

static void json_skip_ws(SimpleJsonParser* p) {
    while (p->pos < p->size) {
        char c = p->data[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else if (c == '/' && p->pos + 1 < p->size) {
            if (p->data[p->pos + 1] == '/') {
                p->pos += 2;
                while (p->pos < p->size && p->data[p->pos] != '\n') p->pos++;
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

static int json_expect_char(SimpleJsonParser* p, char c) {
    json_skip_ws(p);
    if (p->pos >= p->size || p->data[p->pos] != c) return -1;
    p->pos++;
    return 0;
}

static char* json_parse_str(SimpleJsonParser* p) {
    json_skip_ws(p);
    if (p->pos >= p->size || p->data[p->pos] != '"') return NULL;
    p->pos++;
    
    size_t start = p->pos;
    while (p->pos < p->size && p->data[p->pos] != '"') {
        if (p->data[p->pos] == '\\' && p->pos + 1 < p->size) p->pos += 2;
        else p->pos++;
    }
    
    size_t len = p->pos - start;
    char* str = (char*)malloc(len + 1);
    if (!str) return NULL;
    
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
    
    if (p->pos < p->size && p->data[p->pos] == '"') p->pos++;
    return str;
}

static int json_parse_import(SimpleJsonParser* p, ArkLinkSession* session) {
    if (json_expect_char(p, '{') != 0) return -1;
    
    char* module = NULL;
    char* symbol = NULL;
    
    while (1) {
        json_skip_ws(p);
        if (p->pos < p->size && p->data[p->pos] == '}') {
            p->pos++;
            break;
        }
        
        char* key = json_parse_str(p);
        if (!key) return -1;
        
        if (json_expect_char(p, ':') != 0) {
            free(key);
            return -1;
        }
        
        if (strcmp(key, "module") == 0) {
            module = json_parse_str(p);
        } else if (strcmp(key, "symbol") == 0) {
            symbol = json_parse_str(p);
        } else {
            
            json_skip_ws(p);
            if (p->data[p->pos] == '"') {
                char* dummy = json_parse_str(p);
                free(dummy);
            } else {
                while (p->pos < p->size && p->data[p->pos] != ',' && p->data[p->pos] != '}') {
                    p->pos++;
                }
            }
        }
        free(key);
        
        json_skip_ws(p);
        if (p->pos < p->size && p->data[p->pos] == ',') p->pos++;
    }
    
    
    if (module && symbol) {
        arklink_session_add_import(session, module, symbol, 0);
    }
    
    free(module);
    free(symbol);
    return 0;
}

static int json_parse_imports(SimpleJsonParser* p, ArkLinkSession* session) {
    if (json_expect_char(p, '[') != 0) return -1;
    
    json_skip_ws(p);
    if (p->pos < p->size && p->data[p->pos] == ']') {
        p->pos++;
        return 0;
    }
    
    while (1) {
        if (json_parse_import(p, session) != 0) return -1;
        
        json_skip_ws(p);
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

ArkLinkResult arklink_session_load_config(ArkLinkSession* session, const char* config_path) {
    if (!session || !config_path) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    
    FILE* fp = fopen(config_path, "rb");
    if (!fp) {
        return ARK_LINK_ERR_IO;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return ARK_LINK_ERR_IO;
    }
    
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return ARK_LINK_ERR_IO;
    }
    
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return ARK_LINK_ERR_IO;
    }
    
    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(fp);
        return ARK_LINK_ERR_MEMORY;
    }
    
    if (fread(content, 1, (size_t)size, fp) != (size_t)size) {
        free(content);
        fclose(fp);
        return ARK_LINK_ERR_IO;
    }
    content[size] = '\0';
    fclose(fp);
    
    
    SimpleJsonParser parser = {
        .data = content,
        .size = (size_t)size,
        .pos = 0
    };
    
    if (json_expect_char(&parser, '{') != 0) {
        free(content);
        return ARK_LINK_ERR_FORMAT;
    }
    
    while (1) {
        json_skip_ws(&parser);
        if (parser.pos < parser.size && parser.data[parser.pos] == '}') {
            parser.pos++;
            break;
        }
        
        char* key = json_parse_str(&parser);
        if (!key) {
            free(content);
            return ARK_LINK_ERR_FORMAT;
        }
        
        if (json_expect_char(&parser, ':') != 0) {
            free(key);
            free(content);
            return ARK_LINK_ERR_FORMAT;
        }
        
        if (strcmp(key, "runtime") == 0) {
            char* runtime = json_parse_str(&parser);
            if (runtime) {
                
                free(runtime);
            }
        } else if (strcmp(key, "subsystem") == 0) {
            char* subsystem = json_parse_str(&parser);
            if (subsystem) {
                if (strcmp(subsystem, "console") == 0) {
                    session->job.runtime.subsystem = ARK_SUBSYSTEM_CONSOLE;
                } else if (strcmp(subsystem, "windows") == 0) {
                    session->job.runtime.subsystem = ARK_SUBSYSTEM_WINDOWS;
                }
                free(subsystem);
            }
        } else if (strcmp(key, "entry") == 0) {
            char* entry = json_parse_str(&parser);
            if (entry) {
                session->job.runtime.entry_point = entry;
            }
        } else if (strcmp(key, "imageBase") == 0) {
            json_skip_ws(&parser);
            if (parser.pos < parser.size && parser.data[parser.pos] == '"') {
                char* base_str = json_parse_str(&parser);
                if (base_str) {
                    session->job.runtime.image_base = strtoull(base_str, NULL, 0);
                    free(base_str);
                }
            } else {
                session->job.runtime.image_base = 0;
                while (parser.pos < parser.size && 
                       parser.data[parser.pos] >= '0' && parser.data[parser.pos] <= '9') {
                    session->job.runtime.image_base = session->job.runtime.image_base * 10 + 
                                                       (parser.data[parser.pos] - '0');
                    parser.pos++;
                }
            }
        } else if (strcmp(key, "stackSize") == 0) {
            json_skip_ws(&parser);
            session->job.runtime.stack_size = 0;
            while (parser.pos < parser.size && 
                   parser.data[parser.pos] >= '0' && parser.data[parser.pos] <= '9') {
                session->job.runtime.stack_size = session->job.runtime.stack_size * 10 + 
                                                   (parser.data[parser.pos] - '0');
                parser.pos++;
            }
        } else if (strcmp(key, "imports") == 0) {
            if (json_parse_imports(&parser, session) != 0) {
                free(key);
                free(content);
                return ARK_LINK_ERR_FORMAT;
            }
        } else {
            
            json_skip_ws(&parser);
            if (parser.data[parser.pos] == '"') {
                char* dummy = json_parse_str(&parser);
                free(dummy);
            } else if (parser.data[parser.pos] == '[') {
                int depth = 1;
                parser.pos++;
                while (parser.pos < parser.size && depth > 0) {
                    if (parser.data[parser.pos] == '[') depth++;
                    else if (parser.data[parser.pos] == ']') depth--;
                    parser.pos++;
                }
            } else if (parser.data[parser.pos] == '{') {
                int depth = 1;
                parser.pos++;
                while (parser.pos < parser.size && depth > 0) {
                    if (parser.data[parser.pos] == '{') depth++;
                    else if (parser.data[parser.pos] == '}') depth--;
                    parser.pos++;
                }
            } else {
                while (parser.pos < parser.size && 
                       parser.data[parser.pos] != ',' && parser.data[parser.pos] != '}') {
                    parser.pos++;
                }
            }
        }
        
        free(key);
        
        json_skip_ws(&parser);
        if (parser.pos < parser.size && parser.data[parser.pos] == ',') {
            parser.pos++;
        }
    }
    
    free(content);
    return ARK_LINK_OK;
}
