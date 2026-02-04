#include "../../core/utils/es_common.h"
#include "project.h"
#include <time.h>
#include <errno.h>
#include <string.h>


extern size_t strlen(const char *s);
extern int strcmp(const char *s1, const char *s2);
#ifdef _WIN32
#include <direct.h>
#define ES_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define ES_MKDIR(path) mkdir(path, 0775)
#endif

#ifndef ES_MAX_PATH
#define ES_MAX_PATH 1024
#endif

static char es_path_separator(void) {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

static int es_path_is_separator(char c) {
    return (c == '/') || (c == '\\');
}

static void es_join_path(char* buffer, size_t buffer_size, const char* base, const char* part) {
    if (!buffer || buffer_size == 0) return;
    if (!base || base[0] == '\0') {
        snprintf(buffer, buffer_size, "%s", part ? part : "");
        return;
    }
    if (!part || part[0] == '\0') {
        snprintf(buffer, buffer_size, "%s", base);
        return;
    }

    char sep = es_path_separator();
    size_t base_len = strlen(base);
    int need_sep = base_len > 0 && !es_path_is_separator(base[base_len - 1]);

    if (!need_sep) {
        snprintf(buffer, buffer_size, "%s%s", base, part);
    } else {
        snprintf(buffer, buffer_size, "%s%c%s", base, sep, part);
    }
}

static int es_create_directory_recursive(const char* path) {
    if (!path || path[0] == '\0') return 0;

    char temp[ES_MAX_PATH];
    snprintf(temp, sizeof(temp), "%s", path);
    size_t len = strlen(temp);
    if (len == 0) return 0;

    for (size_t i = 0; i < len; ++i) {
        if (es_path_is_separator(temp[i])) {
            char old = temp[i];
            temp[i] = '\0';
            if (temp[0] != '\0') {
                if (ES_MKDIR(temp) != 0 && errno != EEXIST) {
                    temp[i] = old;
                    return -1;
                }
            }
            temp[i] = old;
        }
    }
    if (ES_MKDIR(temp) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void es_write_file_if_possible(const char* path, const char* content) {
    if (!path || !content) return;
    FILE* fp = fopen(path, "w");
    if (!fp) return;
    fputs(content, fp);
    fclose(fp);
}


EsProject* es_proj_create(const char* name, EsProjectType type) {
    EsProject* project = ES_CALLOC(1, sizeof(EsProject));
    if (!project) return NULL;

    project->name = ES_STRDUP(name);
    project->version = ES_STRDUP("1.0.0");
    project->type = type;
    project->output_type = ES_STRDUP(type == ES_PROJ_TYPE_LIBRARY ? "dll" : "exe");
    project->root_namespace = ES_STRDUP(name);
    project->description = ES_STRDUP("");


    project->sdk_version = ES_STRDUP("1.0.0");


    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    project->created_date = ES_STRDUP(time_str);
    project->modified_date = ES_STRDUP(time_str);

    return project;
}


void es_proj_destroy(EsProject* project) {
    if (!project) return;

    ES_FREE(project->name);
    ES_FREE(project->version);
    ES_FREE(project->output_type);
    ES_FREE(project->root_namespace);
    ES_FREE(project->description);
    ES_FREE(project->sdk_version);
    ES_FREE(project->created_date);
    ES_FREE(project->modified_date);
    ES_FREE(project->project_root);


    EsProjectItem* item = project->items;
    while (item) {
        EsProjectItem* next = item->next;
        ES_FREE(item->file_path);
        ES_FREE(item->item_type);
        ES_FREE(item);
        item = next;
    }


    EsProjectDependency* dep = project->dependencies;
    while (dep) {
        EsProjectDependency* next = dep->next;
        ES_FREE(dep->name);
        ES_FREE(dep->version);
        ES_FREE(dep->path);
        ES_FREE(dep);
        dep = next;
    }


    EsProjectPropertyGroup* prop = project->property_groups;
    while (prop) {
        EsProjectPropertyGroup* next = prop->next;
        ES_FREE(prop->output_path);
        ES_FREE(prop->intermediate_path);
        ES_FREE(prop->target_name);
        ES_FREE(prop->defines);
        ES_FREE(prop->include_paths);
        ES_FREE(prop);
        prop = next;
    }

    ES_FREE(project);
}


void es_proj_add_file(EsProject* project, const char* file_path, const char* item_type) {
    if (!project || !file_path) return;

    EsProjectItem* item = ES_CALLOC(1, sizeof(EsProjectItem));
    if (!item) return;

    item->file_path = ES_STRDUP(file_path);
    item->item_type = ES_STRDUP(item_type ? item_type : "Compile");


    item->next = project->items;
    project->items = item;


    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    ES_FREE(project->modified_date);
    project->modified_date = ES_STRDUP(time_str);
}


void es_proj_add_dependency(EsProject* project, const char* name, const char* version, const char* path) {
    if (!project || !name) return;

    EsProjectDependency* dep = ES_CALLOC(1, sizeof(EsProjectDependency));
    if (!dep) return;

    dep->name = ES_STRDUP(name);
    dep->version = ES_STRDUP(version ? version : "*");
    dep->path = path ? ES_STRDUP(path) : NULL;


    dep->next = project->dependencies;
    project->dependencies = dep;
}


char** es_proj_get_source_files(EsProject* project, int* count) {
    if (!project || !count) return NULL;


    int file_count = 0;
    EsProjectItem* item = project->items;
    while (item) {
        if (strcmp(item->item_type, "Compile") == 0) {
            file_count++;
        }
        item = item->next;
    }

    if (file_count == 0) {
        *count = 0;
        return NULL;
    }


    char** files = ES_CALLOC(file_count, sizeof(char*));
    if (!files) {
        *count = 0;
        return NULL;
    }


    int index = 0;
    item = project->items;
    while (item && index < file_count) {
        if (strcmp(item->item_type, "Compile") == 0) {
            files[index++] = ES_STRDUP(item->file_path);
        }
        item = item->next;
    }

    *count = file_count;
    return files;
}


int es_proj_create_template(EsProject* project, const char* output_dir) {
    if (!project || !output_dir) return 0;

    if (output_dir[0] == '\0') {
        output_dir = project->name ? project->name : "MyProject";
    }

    if (!project->project_root) {
        project->project_root = ES_STRDUP(output_dir);
    }

    typedef struct {
        const char* name;
        const char* content;
    } TemplateFile;

    static const TemplateFile console_templates[] = {
        {
            "main.esf",
            "function main() {\n"
            "    print(\"Hello, E#!\");\n"
            "}\n"
        }
    };

    static const TemplateFile library_templates[] = {
        {
            "library.esf",
            "function add(int32 a, int32 b) {\n"
            "    return a + b;\n"
            "}\n"
        },
        {
            "exports.esf",
            "function export_symbols() {\n"
            "}\n"
        }
    };

    static const TemplateFile web_templates[] = {
        {
            "app.esf",
            "function handle_request() {\n"
            "    print(\"HTTP/1.1 200 OK\\r\\n\");\n"
            "    print(\"Content-Type: text/html\\r\\n\\r\\n\");\n"
            "    print(\"<h1>Hello from E# Web!</h1>\");\n"
            "}\n"
        },
        {
            "routes.esf",
            "function configure_routes() {\n"
            "}\n"
        }
    };

    static const TemplateFile system_templates[] = {
        {
            "kernel.esf",
            "function void _start() asm {\n"
            "    mov rax, 1\n"
            "    mov rdi, 1\n"
            "    mov rsi, msg\n"
            "    mov rdx, 13\n"
            "    syscall\n"
            "    mov rax, 60\n"
            "    xor rdi, rdi\n"
            "    syscall\n"
            "}\n\n"
            "section .data\n"
            "    msg: db \"System.Everything is ready\", 10\n\n"
        },
        {
            "drivers.esf",
            "function init_drivers() {\n"
            "}\n"
        }
    };

    const TemplateFile* templates = console_templates;
    size_t template_count = sizeof(console_templates) / sizeof(console_templates[0]);

    switch (project->type) {
        case ES_PROJ_TYPE_LIBRARY:
            templates = library_templates;
            template_count = sizeof(library_templates) / sizeof(library_templates[0]);
            break;
        case ES_PROJ_TYPE_WEB:
            templates = web_templates;
            template_count = sizeof(web_templates) / sizeof(web_templates[0]);
            break;
        case ES_PROJ_TYPE_SYSTEM:
            templates = system_templates;
            template_count = sizeof(system_templates) / sizeof(system_templates[0]);
            break;
        case ES_PROJ_TYPE_CONSOLE:
        default:
            break;
    }

    if (es_create_directory_recursive(output_dir) != 0) {
        return 0;
    }

    for (size_t i = 0; i < template_count; ++i) {
        const TemplateFile* tpl = &templates[i];
        es_proj_add_file(project, tpl->name, "Compile");

        char file_path[ES_MAX_PATH];
        es_join_path(file_path, sizeof(file_path), output_dir, tpl->name);
        es_write_file_if_possible(file_path, tpl->content);
    }

    return 1;
}


char** es_proj_get_templates(int* count) {
    if (!count) return NULL;

    static char* templates[] = {
        "console",
        "library",
        "web",
        "system"
    };

    *count = sizeof(templates) / sizeof(templates[0]);
    return templates;
}

char* es_proj_get_output_path(EsProject* project, EsProjectConfig config) {
    if (!project) return NULL;
    
    
    EsProjectPropertyGroup* prop = project->property_groups;
    while (prop) {
        if (prop->config == config && prop->output_path) {
            return prop->output_path;
        }
        prop = prop->next;
    }
    
    return NULL;
}

char* es_proj_get_intermediate_path(EsProject* project, EsProjectConfig config) {
    if (!project) return NULL;
    
    
    EsProjectPropertyGroup* prop = project->property_groups;
    while (prop) {
        if (prop->config == config && prop->intermediate_path) {
            return prop->intermediate_path;
        }
        prop = prop->next;
    }
    
    return NULL;
}