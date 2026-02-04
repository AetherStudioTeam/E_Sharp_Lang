#ifndef ES_IR_PROFILE_H
#define ES_IR_PROFILE_H

#include "../../../core/utils/es_common.h"
#include <time.h>



typedef enum {
    ES_PROFILE_LEXER,        
    ES_PROFILE_PARSER,       
    ES_PROFILE_SEMANTIC,     
    ES_PROFILE_IR_GEN,       
    ES_PROFILE_IR_OPT,       
    ES_PROFILE_CODEGEN,      
    ES_PROFILE_LINKING,      
    ES_PROFILE_TOTAL,        
    ES_PROFILE_PHASE_COUNT   
} EsProfilePhase;


typedef struct {
    double times[ES_PROFILE_PHASE_COUNT];  
    int counts[ES_PROFILE_PHASE_COUNT];    
    clock_t start_times[ES_PROFILE_PHASE_COUNT];  
    int active[ES_PROFILE_PHASE_COUNT];    
} EsIRProfiler;



typedef struct {
    size_t total_allocated;    
    size_t total_freed;        
    size_t current_used;       
    size_t peak_used;          
    size_t allocation_count;   
    size_t free_count;         
} EsIRMemoryStats;



typedef struct {
    
    int inst_count;            
    int block_count;           
    int function_count;        
    int var_count;             
    
    
    int opt_passes;            
    int opt_applied;           
    
    
    int arena_allocs;          
    int malloc_allocs;         
    size_t arena_bytes;        
} EsIRPerformanceCounters;




extern EsIRProfiler g_ir_profiler;
extern EsIRMemoryStats g_ir_memory_stats;
extern EsIRPerformanceCounters g_ir_counters;


void es_ir_profiler_init(void);
void es_ir_profiler_reset(void);
void es_ir_profiler_print(void);


void es_ir_profile_begin(EsProfilePhase phase);
void es_ir_profile_end(EsProfilePhase phase);


void es_ir_memory_track_alloc(size_t size);
void es_ir_memory_track_free(size_t size);
void es_ir_memory_print(void);


void es_ir_counters_reset(void);
void es_ir_counters_print(void);



#ifdef ENABLE_IR_PROFILING
    #define IR_PROFILE_BEGIN(phase) es_ir_profile_begin(phase)
    #define IR_PROFILE_END(phase) es_ir_profile_end(phase)
    #define IR_TRACK_ALLOC(size) es_ir_memory_track_alloc(size)
    #define IR_TRACK_FREE(size) es_ir_memory_track_free(size)
#else
    #define IR_PROFILE_BEGIN(phase) ((void)0)
    #define IR_PROFILE_END(phase) ((void)0)
    #define IR_TRACK_ALLOC(size) ((void)0)
    #define IR_TRACK_FREE(size) ((void)0)
#endif


#define IR_PROFILE_SCOPE(phase) \
    for (struct { int i; } _ir_profile_scope = (es_ir_profile_begin(phase), (struct { int i; }){0}); \
         _ir_profile_scope.i == 0; \
         es_ir_profile_end(phase), _ir_profile_scope.i = 1)

#endif 
