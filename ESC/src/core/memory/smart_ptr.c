#include "smart_ptr.h"
#include "../utils/es_common.h"
#include "../utils/logger.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>


static struct {
    atomic_size_t total_shared;
    atomic_size_t total_unique;
    atomic_size_t total_weak;
    atomic_size_t total_allocations;
    atomic_size_t total_deallocations;
} g_smart_ptr_stats = {0};


SmartPtr* es_make_shared(const char* type_name, size_t size) {
    if (size == 0) return NULL;
    
    
    SharedControlBlock* control = ES_MALLOC(sizeof(SharedControlBlock));
    if (!control) return NULL;
    
    
    void* data = ES_MALLOC(size);
    if (!data) {
        ES_FREE(control);
        return NULL;
    }
    
    
    atomic_init(&control->ref_count, 1);
    atomic_init(&control->weak_count, 0);
    control->deleter = NULL;
    control->data = data;
    control->is_array = false;
    
    
    SmartPtr* ptr = ES_MALLOC(sizeof(SmartPtr));
    if (!ptr) {
        ES_FREE(data);
        ES_FREE(control);
        return NULL;
    }
    
    ptr->ptr = data;
    ptr->control = control;
    ptr->type = PTR_SHARED;
    ptr->type_name = type_name;
    
    atomic_fetch_add(&g_smart_ptr_stats.total_shared, 1);
    atomic_fetch_add(&g_smart_ptr_stats.total_allocations, 1);
    
    return ptr;
}


SmartPtr* es_make_shared_array(const char* type_name, size_t count, size_t element_size) {
    if (count == 0 || element_size == 0) return NULL;
    
    SmartPtr* ptr = es_make_shared(type_name, count * element_size);
    if (ptr && ptr->control) {
        ptr->control->is_array = true;
    }
    return ptr;
}


SmartPtr* es_make_unique(const char* type_name, size_t size) {
    if (size == 0) return NULL;
    
    void* data = ES_MALLOC(size);
    if (!data) return NULL;
    
    SmartPtr* ptr = ES_MALLOC(sizeof(SmartPtr));
    if (!ptr) {
        ES_FREE(data);
        return NULL;
    }
    
    ptr->ptr = data;
    ptr->control = NULL;
    ptr->type = PTR_UNIQUE;
    ptr->type_name = type_name;
    
    atomic_fetch_add(&g_smart_ptr_stats.total_unique, 1);
    atomic_fetch_add(&g_smart_ptr_stats.total_allocations, 1);
    
    return ptr;
}


SmartPtr* es_make_unique_array(const char* type_name, size_t count, size_t element_size) {
    if (count == 0 || element_size == 0) return NULL;
    return es_make_unique(type_name, count * element_size);
}


void es_shared_acquire(SmartPtr* ptr) {
    if (!ptr || ptr->type != PTR_SHARED || !ptr->control) return;
    
    atomic_fetch_add(&ptr->control->ref_count, 1);
}


void es_shared_release(SmartPtr* ptr) {
    if (!ptr || ptr->type != PTR_SHARED || !ptr->control) return;
    
    int old_count = atomic_fetch_sub(&ptr->control->ref_count, 1);
    
    if (old_count == 1) {
        
        void* data = ptr->control->data;
        
        
        if (ptr->control->deleter) {
            ptr->control->deleter(data);
        } else {
            ES_FREE(data);
        }
        
        
        if (atomic_load(&ptr->control->weak_count) > 0) {
            ptr->control->data = NULL;  
        } else {
            ES_FREE(ptr->control);
            ptr->control = NULL;
        }
        
        atomic_fetch_sub(&g_smart_ptr_stats.total_shared, 1);
        atomic_fetch_add(&g_smart_ptr_stats.total_deallocations, 1);
    }
}


int es_shared_count(const SmartPtr* ptr) {
    if (!ptr || ptr->type != PTR_SHARED || !ptr->control) return 0;
    return atomic_load(&ptr->control->ref_count);
}


void* es_ptr_get(const SmartPtr* ptr) {
    if (!ptr || !es_ptr_is_valid(ptr)) return NULL;
    return ptr->ptr;
}


void* es_ptr_get_checked(const SmartPtr* ptr, const char* file, int line) {
    if (!ptr) {
        fprintf(stderr, "[ES_PTR_ERROR] %s:%d: Null smart pointer\n", file, line);
        return NULL;
    }
    
    if (!es_ptr_is_valid(ptr)) {
        fprintf(stderr, "[ES_PTR_ERROR] %s:%d: Invalid smart pointer (type: %d, name: %s)\n", 
                file, line, ptr->type, ptr->type_name ? ptr->type_name : "unknown");
        return NULL;
    }
    
    return ptr->ptr;
}


bool es_ptr_is_valid(const SmartPtr* ptr) {
    if (!ptr) return false;
    
    switch (ptr->type) {
        case PTR_SHARED:
            return ptr->control && ptr->control->data && 
                   atomic_load(&ptr->control->ref_count) > 0;
        
        case PTR_UNIQUE:
            return ptr->ptr != NULL;
        
        case PTR_WEAK:
            return ptr->control && 
                   atomic_load(&ptr->control->ref_count) > 0;
        
        default:
            return false;
    }
}


bool es_ptr_is_null(const SmartPtr* ptr) {
    return !ptr || !es_ptr_is_valid(ptr) || ptr->ptr == NULL;
}


static void es_ptr_cleanup_internal(SmartPtr* ptr) {
    if (!ptr) return;
    
    switch (ptr->type) {
        case PTR_SHARED:
            es_shared_release(ptr);
            break;
        
        case PTR_UNIQUE:
            if (ptr->ptr) {
                ES_FREE(ptr->ptr);
                atomic_fetch_sub(&g_smart_ptr_stats.total_unique, 1);
                atomic_fetch_add(&g_smart_ptr_stats.total_deallocations, 1);
            }
            break;
        
        case PTR_WEAK:
            if (ptr->control) {
                atomic_fetch_sub(&ptr->control->weak_count, 1);
                
                if (atomic_load(&ptr->control->weak_count) == 0 && 
                    atomic_load(&ptr->control->ref_count) == 0) {
                    ES_FREE(ptr->control);
                }
            }
            atomic_fetch_sub(&g_smart_ptr_stats.total_weak, 1);
            break;
    }
}


void es_ptr_destroy(SmartPtr* ptr) {
    if (!ptr) return;
    es_ptr_cleanup_internal(ptr);
    ES_FREE(ptr);
}


void es_ptr_reset(SmartPtr* ptr, void* new_ptr, size_t size) {
    if (!ptr) return;
    
    
    es_ptr_cleanup_internal(ptr);
    
    
    ptr->ptr = new_ptr;
    ptr->control = NULL;
    ptr->type = PTR_UNIQUE;  
    ptr->type_name = "reset";
}


SmartPtr* es_make_weak(const SmartPtr* shared_ptr) {
    if (!shared_ptr || shared_ptr->type != PTR_SHARED || !shared_ptr->control) {
        return NULL;
    }
    
    SmartPtr* weak = ES_MALLOC(sizeof(SmartPtr));
    if (!weak) return NULL;
    
    weak->ptr = NULL;  
    weak->control = shared_ptr->control;
    weak->type = PTR_WEAK;
    weak->type_name = shared_ptr->type_name;
    
    atomic_fetch_add(&weak->control->weak_count, 1);
    atomic_fetch_add(&g_smart_ptr_stats.total_weak, 1);
    
    return weak;
}


SmartPtr* es_weak_lock(const SmartPtr* weak_ptr) {
    if (!weak_ptr || weak_ptr->type != PTR_WEAK || !weak_ptr->control) {
        return NULL;
    }
    
    
    int current_count = atomic_load(&weak_ptr->control->ref_count);
    while (current_count > 0) {
        if (atomic_compare_exchange_weak(&weak_ptr->control->ref_count, 
                                        &current_count, current_count + 1)) {
            
            SmartPtr* shared = ES_MALLOC(sizeof(SmartPtr));
            if (!shared) {
                atomic_fetch_sub(&weak_ptr->control->ref_count, 1);
                return NULL;
            }
            
            shared->ptr = weak_ptr->control->data;
            shared->control = weak_ptr->control;
            shared->type = PTR_SHARED;
            shared->type_name = weak_ptr->type_name;
            
            atomic_fetch_add(&g_smart_ptr_stats.total_shared, 1);
            return shared;
        }
    }
    
    return NULL;  
}


bool es_weak_expired(const SmartPtr* weak_ptr) {
    if (!weak_ptr || weak_ptr->type != PTR_WEAK || !weak_ptr->control) {
        return true;
    }
    
    return atomic_load(&weak_ptr->control->ref_count) == 0;
}


void es_ptr_dump_stats(void) {
    printf("=== Smart Pointer Statistics ===\n");
    printf("Active Shared Pointers: %zu\n", g_smart_ptr_stats.total_shared);
    printf("Active Unique Pointers: %zu\n", g_smart_ptr_stats.total_unique);
    printf("Active Weak Pointers: %zu\n", g_smart_ptr_stats.total_weak);
    printf("Total Allocations: %zu\n", g_smart_ptr_stats.total_allocations);
    printf("Total Deallocations: %zu\n", g_smart_ptr_stats.total_deallocations);
    printf("Potential Leaks: %zu\n", 
           g_smart_ptr_stats.total_allocations - g_smart_ptr_stats.total_deallocations);
    printf("================================\n");
}


size_t es_ptr_get_total_shared_count(void) {
    return g_smart_ptr_stats.total_shared;
}

size_t es_ptr_get_total_unique_count(void) {
    return g_smart_ptr_stats.total_unique;
}