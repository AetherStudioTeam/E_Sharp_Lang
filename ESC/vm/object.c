#include <string.h>
#include "object.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocate_object(vm, sizeof(type), objectType)

static struct EsObject* allocate_object(EsVM* vm, size_t size, EsObjType type) {
    struct EsObject* object = (struct EsObject*)es_vm_reallocate(vm, NULL, 0, size);
    object->type = type;
    object->is_marked = false;
    
    
    object->next = vm->objects;
    vm->objects = object;
    
    return object;
}

EsString* es_object_new_string(void* vm, const char* chars, int length) {
    char* heap_chars = (char*)es_vm_reallocate((EsVM*)vm, NULL, 0, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return es_object_take_string(vm, heap_chars, length);
}

EsString* es_object_take_string(void* vm, char* chars, int length) {
    EsString* string = ALLOCATE_OBJ((EsVM*)vm, EsString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}
