#include <string.h>
#include <stdlib.h>
#include "../../core/utils/es_common.h"
#include "project.h"


extern char *strncpy(char *dest, const char *src, size_t n);
extern int strcmp(const char *s1, const char *s2);
extern char *strstr(const char *haystack, const char *needle);
extern size_t strlen(const char *s);
extern char *strchr(const char *s, int c);
extern char *strrchr(const char *s, int c);

static char* es_extract_directory(const char* path) {
    if (!path) return NULL;
    const char* last_forward = strrchr(path, '/');
    const char* last_backward = strrchr(path, '\\');
    const char* last = last_forward > last_backward ? last_forward : last_backward;
    if (!last) {
        return ES_STRDUP(".");
    }
    size_t len = (size_t)(last - path);
    if (len == 0) {
        return ES_STRDUP(".");
    }
    char* dir = ES_MALLOC(len + 1);
    if (!dir) return NULL;
    strncpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}


static char* xml_get_attribute(const char* xml_content, const char* element_name, const char* attr_name) {
    if (!xml_content || !element_name || !attr_name ||
        element_name[0] == '\0' || attr_name[0] == '\0') {
        return NULL;
    }

    char element_tag[256];
    int element_tag_len = snprintf(element_tag, sizeof(element_tag), "<%s", element_name);
    if (element_tag_len < 0 || element_tag_len >= sizeof(element_tag)) {
        return NULL;
    }

    const char* element_start = strstr(xml_content, element_tag);
    if (!element_start) return NULL;

    char attr_pattern[256];
    int attr_pattern_len = snprintf(attr_pattern, sizeof(attr_pattern), "%s=\"", attr_name);
    if (attr_pattern_len < 0 || attr_pattern_len >= sizeof(attr_pattern)) {
        return NULL;
    }

    const char* attr_start = strstr(element_start, attr_pattern);
    if (!attr_start) return NULL;

    attr_start += strlen(attr_pattern);
    const char* attr_end = strchr(attr_start, '"');
    if (!attr_end) return NULL;

    size_t attr_len = attr_end - attr_start;
    if (attr_len == 0) {

        char* result = ES_MALLOC(1);
        if (result) {
            result[0] = '\0';
        }
        return result;
    }

    char* result = ES_MALLOC(attr_len + 1);
    if (result) {
        strncpy(result, attr_start, attr_len);
        result[attr_len] = '\0';
    }

    return result;
}

static char* xml_get_element_content(const char* xml_content, const char* element_name) {
    if (!xml_content || !element_name || element_name[0] == '\0') {
        return NULL;
    }

    char start_tag[256];
    int start_tag_len = snprintf(start_tag, sizeof(start_tag), "<%s>", element_name);
    if (start_tag_len < 0 || start_tag_len >= sizeof(start_tag)) {
        return NULL;
    }

    const char* content_start = strstr(xml_content, start_tag);
    if (!content_start) {

        char self_closing_tag[256];
        int self_closing_len = snprintf(self_closing_tag, sizeof(self_closing_tag), "<%s ", element_name);
        if (self_closing_len > 0 && self_closing_len < sizeof(self_closing_tag)) {
            content_start = strstr(xml_content, self_closing_tag);
            if (content_start) {

                char* result = ES_MALLOC(1);
                if (result) {
                    result[0] = '\0';
                }
                return result;
            }
        }
        return NULL;
    }

    content_start += strlen(start_tag);

    char end_tag[256];
    int end_tag_len = snprintf(end_tag, sizeof(end_tag), "</%s>", element_name);
    if (end_tag_len < 0 || end_tag_len >= sizeof(end_tag)) {
        return NULL;
    }

    const char* content_end = strstr(content_start, end_tag);
    if (!content_end) return NULL;

    size_t content_len = content_end - content_start;
    if (content_len == 0) {

        char* result = ES_MALLOC(1);
        if (result) {
            result[0] = '\0';
        }
        return result;
    }

    char* result = ES_MALLOC(content_len + 1);
    if (result) {
        strncpy(result, content_start, content_len);
        result[content_len] = '\0';
    }

    return result;
}


EsProject* es_proj_load(const char* proj_file) {
    if (!proj_file || proj_file[0] == '\0') {
        ES_ERROR("项目文件路径不能为空");
        return NULL;
    }

    FILE* fp = fopen(proj_file, "r");
    if (!fp) {
        ES_ERROR("无法打开项目文件: %s", proj_file);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);


    if (file_size <= 0) {
        ES_ERROR("项目文件为空: %s", proj_file);
        fclose(fp);
        return NULL;
    }

    char* content = ES_MALLOC(file_size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(content, 1, file_size, fp);
    content[read_size] = '\0';
    fclose(fp);


    if (read_size == 0 || content[0] == '\0') {
        ES_ERROR("项目文件内容为空: %s", proj_file);
        ES_FREE(content);
        return NULL;
    }


    if (!content || strlen(content) == 0) {
        ES_ERROR("项目文件为空: %s", proj_file);
        ES_FREE(content);
        return NULL;
    }


    char* name = xml_get_element_content(content, "ProjectName");
    EsProject* project = NULL;

    if (name && name[0] != '\0') {
        project = es_proj_create(name, ES_PROJ_TYPE_CONSOLE);
        ES_FREE(name);
    } else {

        const char* base_name = strrchr(proj_file, '/');
        if (!base_name) base_name = strrchr(proj_file, '\\');
        if (!base_name) base_name = proj_file;
        else base_name++;

        char project_name[256];
        const char* ext = strrchr(base_name, '.');
        if (ext) {
            size_t name_len = ext - base_name;
            if (name_len >= sizeof(project_name)) name_len = sizeof(project_name) - 1;
            strncpy(project_name, base_name, name_len);
            project_name[name_len] = '\0';
        } else {
            strncpy(project_name, base_name, sizeof(project_name) - 1);
            project_name[sizeof(project_name) - 1] = '\0';
        }

        project = es_proj_create(project_name, ES_PROJ_TYPE_CONSOLE);
    }


    if (!project || !project->name || project->name[0] == '\0') {
        ES_ERROR("项目文件无效: 项目名称不能为空");
        if (project) {
            es_proj_destroy(project);
        }
        ES_FREE(content);
        return NULL;
    }

    if (!project) {
        ES_FREE(content);
        return NULL;
    }

    if (project->project_root) {
        ES_FREE(project->project_root);
        project->project_root = NULL;
    }
    project->project_root = es_extract_directory(proj_file);


    char* type_str = xml_get_element_content(content, "ProjectType");
    if (type_str) {
        if (strcmp(type_str, "Console") == 0) project->type = ES_PROJ_TYPE_CONSOLE;
        else if (strcmp(type_str, "Library") == 0) project->type = ES_PROJ_TYPE_LIBRARY;
        else if (strcmp(type_str, "Web") == 0) project->type = ES_PROJ_TYPE_WEB;
        else if (strcmp(type_str, "System") == 0) project->type = ES_PROJ_TYPE_SYSTEM;
        ES_FREE(type_str);
    }


    char* version = xml_get_element_content(content, "Version");
    if (version) {
        ES_FREE(project->version);
        project->version = version;
    }


    char* output_type = xml_get_element_content(content, "OutputType");
    if (output_type) {
        ES_FREE(project->output_type);
        project->output_type = output_type;
    }


    char* description = xml_get_element_content(content, "Description");
    if (description) {
        ES_FREE(project->description);
        project->description = description;
    }


    char* item_group = xml_get_element_content(content, "ItemGroup");
    if (item_group) {

        const char* compile_start = item_group;
        while ((compile_start = strstr(compile_start, "<Compile")) != NULL) {
            char* file_path = xml_get_attribute(compile_start, "Compile", "Include");
            if (file_path) {
                es_proj_add_file(project, file_path, "Compile");
                ES_FREE(file_path);
            } else {
                ES_WARNING("Compile 元素没有 Include 属性");
            }
            compile_start += 8;
        }
        ES_FREE(item_group);
    }


    char* dependencies = xml_get_element_content(content, "Dependencies");
    if (dependencies) {

        const char* dep_start = dependencies;
        while ((dep_start = strstr(dep_start, "<PackageReference")) != NULL) {
            char* name = xml_get_attribute(dep_start, "PackageReference", "Include");
            char* version = xml_get_attribute(dep_start, "PackageReference", "Version");
            if (name) {
                es_proj_add_dependency(project, name, version ? version : "*", NULL);
                ES_FREE(name);
                if (version) ES_FREE(version);
            }
            dep_start += 18;
        }
        ES_FREE(dependencies);
    }


    char* property_group = xml_get_element_content(content, "PropertyGroup");
    if (property_group) {
        EsProjectPropertyGroup* prop = ES_CALLOC(1, sizeof(EsProjectPropertyGroup));
        if (prop) {
            prop->config = ES_PROJ_CONFIG_DEBUG;

            char* output_path = xml_get_element_content(property_group, "OutputPath");
            if (output_path) {
                prop->output_path = output_path;
            }

            char* target_name = xml_get_element_content(property_group, "AssemblyName");
            if (target_name) {
                prop->target_name = target_name;
            }

            char* defines = xml_get_element_content(property_group, "DefineConstants");
            if (defines) {
                prop->defines = defines;
            }


            prop->next = project->property_groups;
            project->property_groups = prop;
        }
        ES_FREE(property_group);
    }

    ES_FREE(content);
    return project;
}


int es_proj_save(EsProject* project, const char* proj_file) {
    if (!project || !proj_file) return -1;

    FILE* fp = fopen(proj_file, "w");
    if (!fp) {
        ES_ERROR("无法创建项目文件: %s", proj_file);
        return -1;
    }


    fprintf(fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    fprintf(fp, "<Project Sdk=\"E# SDK\">\n");
    fprintf(fp, "  <PropertyGroup>\n");
    fprintf(fp, "    <ProjectName>%s</ProjectName>\n", project->name);

    const char* type_str = "Console";
    switch (project->type) {
        case ES_PROJ_TYPE_LIBRARY: type_str = "Library"; break;
        case ES_PROJ_TYPE_WEB: type_str = "Web"; break;
        case ES_PROJ_TYPE_SYSTEM: type_str = "System"; break;
        default: break;
    }
    fprintf(fp, "    <ProjectType>%s</ProjectType>\n", type_str);

    if (project->version) {
        fprintf(fp, "    <Version>%s</Version>\n", project->version);
    }

    if (project->output_type) {
        fprintf(fp, "    <OutputType>%s</OutputType>\n", project->output_type);
    }

    if (project->description) {
        fprintf(fp, "    <Description>%s</Description>\n", project->description);
    }

    if (project->sdk_version) {
        fprintf(fp, "    <ES_SDK>%s</ES_SDK>\n", project->sdk_version);
    }

    fprintf(fp, "  </PropertyGroup>\n");


    EsProjectPropertyGroup* prop = project->property_groups;
    while (prop) {
        fprintf(fp, "  <PropertyGroup Condition=\"'$(Configuration)'=='%s'\">\n",
                prop->config == ES_PROJ_CONFIG_DEBUG ? "Debug" : "Release");

        if (prop->output_path) {
            fprintf(fp, "    <OutputPath>%s</OutputPath>\n", prop->output_path);
        }

        if (prop->target_name) {
            fprintf(fp, "    <AssemblyName>%s</AssemblyName>\n", prop->target_name);
        }

        if (prop->defines) {
            fprintf(fp, "    <DefineConstants>%s</DefineConstants>\n", prop->defines);
        }

        fprintf(fp, "    <Optimize>%s</Optimize>\n", prop->optimize ? "true" : "false");

        fprintf(fp, "  </PropertyGroup>\n");
        prop = prop->next;
    }


    if (project->items) {
        fprintf(fp, "  <ItemGroup>\n");

        EsProjectItem* item = project->items;
        while (item) {
            fprintf(fp, "    <%s Include=\"%s\" />\n", item->item_type, item->file_path);
            item = item->next;
        }

        fprintf(fp, "  </ItemGroup>\n");
    }


    if (project->dependencies) {
        fprintf(fp, "  <ItemGroup>\n");

        EsProjectDependency* dep = project->dependencies;
        while (dep) {
            fprintf(fp, "    <PackageReference Include=\"%s\" Version=\"%s\"", dep->name, dep->version);
            if (dep->path) {
                fprintf(fp, " Path=\"%s\"", dep->path);
            }
            fprintf(fp, " />\n");
            dep = dep->next;
        }

        fprintf(fp, "  </ItemGroup>\n");
    }

    fprintf(fp, "</Project>\n");
    fclose(fp);

    return 0;
}