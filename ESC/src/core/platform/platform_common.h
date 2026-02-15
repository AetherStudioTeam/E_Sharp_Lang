#ifndef PLATFORM_H
#define PLATFORM_H


#ifdef _WIN32
    #define ES_PLATFORM_WINDOWS 1
    #define ES_PLATFORM_LINUX 0
#elif defined(__linux__)
    #define ES_PLATFORM_WINDOWS 0
    #define ES_PLATFORM_LINUX 1
#else
    #error "Unsupported platform"
#endif


#ifdef _WIN32
    #define ES_EXPORT __declspec(dllexport)
    #define ES_IMPORT __declspec(dllimport)
    #define ES_API __stdcall
#else
    
    #define ES_EXPORT 
    #define ES_IMPORT 
    #define ES_API 
#endif


#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE es_handle_t;
    typedef DWORD es_dword_t;
    typedef WORD es_word_t;
    #define ES_INVALID_HANDLE INVALID_HANDLE_VALUE
#else
    #include <unistd.h>
    #include <sys/types.h>
    typedef int es_handle_t;
    typedef unsigned int es_dword_t;
    typedef unsigned short es_word_t;
    #define ES_INVALID_HANDLE -1
#endif


#ifdef _WIN32
    #define ES_STDIN_HANDLE GetStdHandle(STD_INPUT_HANDLE)
    #define ES_STDOUT_HANDLE GetStdHandle(STD_OUTPUT_HANDLE)
    #define ES_STDERR_HANDLE GetStdHandle(STD_ERROR_HANDLE)
#else
    #define ES_STDIN_HANDLE 0  
    #define ES_STDOUT_HANDLE 1 
    #define ES_STDERR_HANDLE 2 
#endif


#ifdef _WIN32
    #define es_write_console(handle, buffer, count, written) \
        do { DWORD _written; WriteConsole(handle, buffer, count, &_written, NULL); *(written) = _written; } while(0)
    #define es_read_console(handle, buffer, count, read, ctrl) \
        do { DWORD _read; ReadConsole(handle, buffer, count, &_read, ctrl); *(read) = _read; } while(0)
    #define es_write_file(handle, buffer, count, written, overlapped) \
        do { DWORD _written; WriteFile(handle, buffer, count, &_written, overlapped); *(written) = _written; } while(0)
    #define es_read_file(handle, buffer, count, read, overlapped) \
        do { DWORD _read; ReadFile(handle, buffer, count, &_read, overlapped); *(read) = _read; } while(0)
#else
    #include <sys/syscall.h>
    #define es_write_console(handle, buffer, count, written) \
        (*(written) = write(handle, buffer, count))
    #define es_read_console(handle, buffer, count, read, ctrl) \
        (*(read) = read(handle, buffer, count))
    #define es_write_file(handle, buffer, count, written, overlapped) \
        (*(written) = write(handle, buffer, count))
    #define es_read_file(handle, buffer, count, read, overlapped) \
        (*(read) = read(handle, buffer, count))
#endif


#ifdef _WIN32
    #ifndef _WINDOWS_
        #include <windows.h>
    #endif
    #define es_malloc_impl(size) HeapAlloc(GetProcessHeap(), 0, size)
    #define es_free_impl(ptr) HeapFree(GetProcessHeap(), 0, ptr)
    #define es_realloc_impl(ptr, size) HeapReAlloc(GetProcessHeap(), 0, ptr, size)
    #define es_calloc_impl(num, size) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (num) * (size))
#else
    #include <unistd.h>
    #include <sys/mman.h>
    
    void* es_malloc_impl(size_t size);
    void es_free_impl(void* ptr);
    void* es_realloc_impl(void* ptr, size_t size);
    void* es_calloc_impl(size_t num, size_t size);
#endif


#ifdef _WIN32
    #include <process.h>
    typedef HANDLE es_thread_t;
    typedef DWORD es_thread_id_t;
    #define es_thread_create(handle, routine, arg) \
        (*(handle) = (HANDLE)_beginthreadex(NULL, 0, routine, arg, 0, NULL))
    #define es_thread_wait(handle) WaitForSingleObject(handle, INFINITE)
    #define es_thread_close(handle) CloseHandle(handle)
    #define es_mutex_init(mutex) InitializeCriticalSection(mutex)
    #define es_mutex_lock(mutex) EnterCriticalSection(mutex)
    #define es_mutex_unlock(mutex) LeaveCriticalSection(mutex)
    #define es_mutex_destroy(mutex) DeleteCriticalSection(mutex)
    typedef CRITICAL_SECTION es_mutex_t;
#else
    #include <pthread.h>
    typedef pthread_t es_thread_t;
    typedef pthread_t es_thread_id_t;
    #define es_thread_create(handle, routine, arg) \
        pthread_create(handle, NULL, routine, arg)
    #define es_thread_wait(handle) pthread_join(handle, NULL)
    #define es_thread_close(handle) 
    #define es_mutex_init(mutex) pthread_mutex_init(mutex, NULL)
    #define es_mutex_lock(mutex) pthread_mutex_lock(mutex)
    #define es_mutex_unlock(mutex) pthread_mutex_unlock(mutex)
    #define es_mutex_destroy(mutex) pthread_mutex_destroy(mutex)
    typedef pthread_mutex_t es_mutex_t;
#endif


#ifdef _WIN32
    #include <time.h>
    #define es_time_ms() GetTickCount64()
    #define es_time_us() (GetTickCount64() * 1000)
    #define es_sleep_ms(ms) Sleep(ms)
#else
    #include <sys/time.h>
    #include <unistd.h>
    static inline unsigned long long es_time_ms(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
    }
    #define es_time_us() (es_time_ms() * 1000)
    #define es_sleep_ms(ms) usleep((ms) * 1000)
#endif


#ifdef _WIN32
    #define es_file_exists(path) (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
    #define es_directory_exists(path) \
        (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES && \
         GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY)
    #define es_create_directory(path) CreateDirectory(path, NULL)
    #define es_remove_directory(path) RemoveDirectory(path)
    #define es_delete_file(path) DeleteFile(path)
    #define ES_PATH_SEPARATOR '\\'
    #define ES_PATH_SEPARATOR_STR "\\"
#else
    #include <sys/stat.h>
    #include <unistd.h>
    static inline int es_file_exists(const char* path) {
        struct stat st;
        return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
    }
    static inline int es_directory_exists(const char* path) {
        struct stat st;
        return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
    }
    #define es_create_directory(path) mkdir(path, 0755)
    #define es_remove_directory(path) rmdir(path)
    #define es_delete_file(path) unlink(path)
    #define ES_PATH_SEPARATOR '/'
    #define ES_PATH_SEPARATOR_STR "/"
#endif

#ifdef _WIN32
    #define es_load_library(name) LoadLibrary(name)
    #define es_free_library(handle) FreeLibrary(handle)
    #define es_get_proc_address(handle, name) GetProcAddress(handle, name)
    typedef HMODULE es_lib_handle_t;
#else
    #include <dlfcn.h>
    #define es_load_library(name) dlopen(name, RTLD_LAZY)
    #define es_free_library(handle) dlclose(handle)
    #define es_get_proc_address(handle, name) dlsym(handle, name)
    typedef void* es_lib_handle_t;
#endif

#ifdef _WIN32
    #define es_atomic_inc(ptr) InterlockedIncrement(ptr)
    #define es_atomic_dec(ptr) InterlockedDecrement(ptr)
    #define es_atomic_add(ptr, val) InterlockedExchangeAdd(ptr, val)
    #define es_atomic_xchg(ptr, val) InterlockedExchange(ptr, val)
    #define es_atomic_cmp_xchg(ptr, cmp, xchg) InterlockedCompareExchange(ptr, xchg, cmp)
#else
    #include <stdatomic.h>
    #define es_atomic_inc(ptr) __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
    #define es_atomic_dec(ptr) __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST)
    #define es_atomic_add(ptr, val) __atomic_add_fetch(ptr, val, __ATOMIC_SEQ_CST)
    #define es_atomic_xchg(ptr, val) __atomic_exchange_n(ptr, val, __ATOMIC_SEQ_CST)
    #define es_atomic_cmp_xchg(ptr, cmp, xchg) \
        __atomic_compare_exchange_n(ptr, cmp, xchg, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#endif

#ifdef _WIN32
    #define es_get_last_error() GetLastError()
    #define es_set_last_error(err) SetLastError(err)
    #define ES_ERROR_SUCCESS 0
    #define ES_ERROR_INVALID_PARAMETER ERROR_INVALID_PARAMETER
    #define ES_ERROR_FILE_NOT_FOUND ERROR_FILE_NOT_FOUND
    #define ES_ERROR_ACCESS_DENIED ERROR_ACCESS_DENIED
    #define ES_ERROR_OUT_OF_MEMORY ERROR_NOT_ENOUGH_MEMORY
#else
    #include <errno.h>
    #define es_get_last_error() errno
    #define es_set_last_error(err) errno = err
    #define ES_ERROR_SUCCESS 0
    #define ES_ERROR_INVALID_PARAMETER EINVAL
    #define ES_ERROR_FILE_NOT_FOUND ENOENT
    #define ES_ERROR_ACCESS_DENIED EACCES
    #define ES_ERROR_OUT_OF_MEMORY ENOMEM
#endif

#ifdef _WIN32
    #define ES_INLINE __forceinline
    #define ES_NOINLINE __declspec(noinline)
    #define ES_PACKED __pragma(pack(push, 1))
    #define ES_UNPACKED __pragma(pack(pop))
    #define ES_ALIGNED(n) __declspec(align(n))
#else
    #define ES_INLINE static inline
    #define ES_NOINLINE __attribute__((noinline))
    #define ES_PACKED __attribute__((packed))
    #define ES_UNPACKED
    #define ES_ALIGNED(n) __attribute__((aligned(n)))
#endif

#ifdef _WIN32
    #define ES_LINKER_OUTPUT_FLAG "-o"
    #define ES_LINKER_CONSOLE_FLAG "-subsystem:console"
    #define ES_LINKER_ENTRY_FLAG "-entry:main"
    #define ES_LINKER_LIB_PATH_FLAG "-libpath:"
    #define ES_LINKER_RUNTIME_LIBS "kernel32.lib user32.lib msvcrt.lib"
    #define ES_LINKER_CMD "ld.exe"
#else
    #define ES_LINKER_OUTPUT_FLAG "-o"
    #define ES_LINKER_CONSOLE_FLAG
    #define ES_LINKER_ENTRY_FLAG "-e"
    #define ES_LINKER_LIB_PATH_FLAG "-L"
    #define ES_LINKER_RUNTIME_LIBS "-lc -lm"
    #define ES_LINKER_CMD "ld"
#endif

#ifdef _WIN32
    #define ES_ASM_FORMAT "win64"
    #define ES_ASM_OUTPUT_FLAG "-f"
    #define ES_ASM_CMD "nasm.exe"
#else
    #define ES_ASM_FORMAT "elf64"
    #define ES_ASM_OUTPUT_FLAG "-f"
    #define ES_ASM_CMD "nasm"
#endif

#endif