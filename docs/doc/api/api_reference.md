# E# API 参考文档

## 目录

1. [输入输出](#输入输出)
2. [内存管理](#内存管理)
3. [字符串操作](#字符串操作)
4. [数学函数](#数学函数)
5. [时间与日期](#时间与日期)
6. [文件操作](#文件操作)
7. [数组操作](#数组操作)
8. [哈希表操作](#哈希表操作)
9. [网络编程](#网络编程)
10. [多线程](#多线程)
11. [图形界面](#图形界面)
12. [错误处理](#错误处理)

## 输入输出

### _print_string

打印字符串到控制台。

```csharp
function _print_string(str: string): void
```

**参数:**
- `str`: 要打印的字符串

**示例:**
```csharp
_print_string("Hello, World!");
```

### _print_number

打印整数到控制台。

```csharp
function _print_number(num: int): void
```

**参数:**
- `num`: 要打印的整数

**示例:**
```csharp
_print_number(42);
```

### _print_float

打印浮点数到控制台，自动处理精度和特殊值。

```csharp
function _print_float(num: float64): void
```

**参数:**
- `num`: 要打印的浮点数

**示例:**
```csharp
_print_float(3.14159);
_print_float(1.0 / 0.0);  // 打印 Infinity
_print_float(0.0 / 0.0);  // 打印 NaN
```

### _print_newline

打印换行符。

```csharp
function _print_newline(): void
```

**示例:**
```csharp
_print_string("Line 1");
_print_newline();
_print_string("Line 2");
```

### _read_char

从控制台读取一个字符。

```csharp
function _read_char(): int
```

**返回值:**
- 读取的字符的ASCII码值

**示例:**
```csharp
var c = _read_char();
_print_char(c);
```

### Console__WriteLine

向控制台写入一行文本。

```csharp
function Console__WriteLine(str: string): void
```

**参数:**
- `str`: 要写入的字符串

**示例:**
```csharp
Console__WriteLine("Hello, E#!");
```

### Console__Write

向控制台写入文本，不换行。

```csharp
function Console__Write(str: string): void
```

**参数:**
- `str`: 要写入的字符串

**示例:**
```csharp
Console__Write("Hello, ");
Console__Write("E#!");
```

## 内存管理

### es_malloc

分配指定大小的内存块。

```csharp
function es_malloc(size: size_t): void*
```

**参数:**
- `size`: 要分配的内存大小（字节）

**返回值:**
- 指向分配的内存块的指针，如果分配失败则返回null

**示例:**
```csharp
var ptr = es_malloc(1024);
if (ptr != null) {
    // 使用内存
    es_free(ptr);
}
```

### es_free

释放之前分配的内存块。

```csharp
function es_free(ptr: void*): void
```

**参数:**
- `ptr`: 要释放的内存块指针

**示例:**
```csharp
var ptr = es_malloc(1024);
// 使用内存
es_free(ptr);
```

### es_realloc

重新分配内存块大小。

```csharp
function es_realloc(ptr: void*, size: size_t): void*
```

**参数:**
- `ptr`: 原始内存块指针
- `size`: 新的内存大小（字节）

**返回值:**
- 指向重新分配的内存块的指针，如果分配失败则返回null

**示例:**
```csharp
var ptr = es_malloc(512);
ptr = es_realloc(ptr, 1024);
if (ptr != null) {
    // 使用更大的内存
    es_free(ptr);
}
```

### es_calloc

分配并初始化为零的内存块。

```csharp
function es_calloc(num: size_t, size: size_t): void*
```

**参数:**
- `num`: 元素数量
- `size`: 每个元素的大小（字节）

**返回值:**
- 指向分配并初始化为零的内存块的指针，如果分配失败则返回null

**示例:**
```csharp
var ptr = es_calloc(10, 4);  // 分配10个4字节的元素，全部初始化为0
if (ptr != null) {
    // 使用内存
    es_free(ptr);
}
```

### get_total_memory

获取系统总内存大小。

```csharp
function get_total_memory(): uint64
```

**返回值:**
- 系统总内存大小（字节）

**示例:**
```csharp
var total = get_total_memory();
_print_string("Total memory: ");
_print_number(total / 1024 / 1024);
_print_string(" MB");
```

### get_free_memory

获取系统可用内存大小。

```csharp
function get_free_memory(): uint64
```

**返回值:**
- 系统可用内存大小（字节）

**示例:**
```csharp
var free = get_free_memory();
_print_string("Free memory: ");
_print_number(free / 1024 / 1024);
_print_string(" MB");
```

## 字符串操作

### es_strlen

计算字符串长度。

```csharp
function es_strlen(str: string): size_t
```

**参数:**
- `str`: 要计算长度的字符串

**返回值:**
- 字符串的长度（不包括终止空字符）

**示例:**
```csharp
var len = es_strlen("Hello");
_print_number(len);  // 输出 5
```

### es_strcpy

复制字符串。

```csharp
function es_strcpy(dest: string, src: string): string
```

**参数:**
- `dest`: 目标字符串缓冲区
- `src`: 源字符串

**返回值:**
- 目标字符串的指针

**示例:**
```csharp
var dest = es_malloc(100);
es_strcpy(dest, "Hello, World!");
_print_string(dest);
es_free(dest);
```

### es_strcat

连接字符串。

```csharp
function es_strcat(dest: string, src: string): string
```

**参数:**
- `dest`: 目标字符串（必须有足够的空间）
- `src`: 要追加的源字符串

**返回值:**
- 目标字符串的指针

**示例:**
```csharp
var dest = es_malloc(100);
es_strcpy(dest, "Hello, ");
es_strcat(dest, "World!");
_print_string(dest);
es_free(dest);
```

### es_strcmp

比较两个字符串。

```csharp
function es_strcmp(str1: string, str2: string): int
```

**参数:**
- `str1`: 第一个字符串
- `str2`: 第二个字符串

**返回值:**
- 如果str1 < str2，返回负数
- 如果str1 == str2，返回0
- 如果str1 > str2，返回正数

**示例:**
```csharp
var result = es_strcmp("apple", "banana");
if (result < 0) {
    _print_string("apple comes before banana");
}
```

### es_strdup

复制字符串（动态分配内存）。

```csharp
function es_strdup(src: string): string
```

**参数:**
- `src`: 要复制的源字符串

**返回值:**
- 新分配的字符串副本，使用后需要用es_free释放

**示例:**
```csharp
var copy = es_strdup("Hello, World!");
_print_string(copy);
es_free(copy);
```

### es_strchr

在字符串中查找字符。

```csharp
function es_strchr(str: string, c: int): string
```

**参数:**
- `str`: 要搜索的字符串
- `c`: 要查找的字符

**返回值:**
- 指向字符串中第一次出现字符c的位置的指针，如果未找到则返回null

**示例:**
```csharp
var pos = es_strchr("Hello, World!", 'W');
if (pos != null) {
    _print_string(pos);  // 输出 "World!"
}
```

### es_strstr

在字符串中查找子字符串。

```csharp
function es_strstr(haystack: string, needle: string): string
```

**参数:**
- `haystack`: 要搜索的字符串
- `needle`: 要查找的子字符串

**返回值:**
- 指向字符串中第一次出现子字符串的位置的指针，如果未找到则返回null

**示例:**
```csharp
var pos = es_strstr("Hello, World!", "World");
if (pos != null) {
    _print_string(pos);  // 输出 "World!"
}
```

### es_atoi

将字符串转换为整数。

```csharp
function es_atoi(str: string): int
```

**参数:**
- `str`: 要转换的字符串

**返回值:**
- 转换后的整数值

**示例:**
```csharp
var num = es_atoi("123");
_print_number(num);  // 输出 123
```

### es_atof

将字符串转换为浮点数。

```csharp
function es_atof(str: string): double
```

**参数:**
- `str`: 要转换的字符串

**返回值:**
- 转换后的浮点数值

**示例:**
```csharp
var num = es_atof("3.14159");
_print_float(num);  // 输出 3.14159
```

## 数学函数

### es_sin

计算正弦值。

```csharp
function es_sin(x: double): double
```

**参数:**
- `x`: 角度（弧度）

**返回值:**
- 正弦值

**示例:**
```csharp
var result = es_sin(3.14159 / 2);
_print_float(result);  // 输出约 1.0
```

### es_cos

计算余弦值。

```csharp
function es_cos(x: double): double
```

**参数:**
- `x`: 角度（弧度）

**返回值:**
- 余弦值

**示例:**
```csharp
var result = es_cos(0);
_print_float(result);  // 输出 1.0
```

### es_tan

计算正切值。

```csharp
function es_tan(x: double): double
```

**参数:**
- `x`: 角度（弧度）

**返回值:**
- 正切值

**示例:**
```csharp
var result = es_tan(3.14159 / 4);
_print_float(result);  // 输出约 1.0
```

### es_sqrt

计算平方根。

```csharp
function es_sqrt(x: double): double
```

**参数:**
- `x`: 要计算平方根的数值

**返回值:**
- 平方根值

**示例:**
```csharp
var result = es_sqrt(16);
_print_float(result);  // 输出 4.0
```

### es_pow

计算幂。

```csharp
function es_pow(base: double, exp: double): double
```

**参数:**
- `base`: 底数
- `exp`: 指数

**返回值:**
- base的exp次幂

**示例:**
```csharp
var result = es_pow(2, 3);
_print_float(result);  // 输出 8.0
```

### es_log

计算自然对数。

```csharp
function es_log(x: double): double
```

**参数:**
- `x`: 要计算对数的数值

**返回值:**
- 自然对数值

**示例:**
```csharp
var result = es_log(2.71828);
_print_float(result);  // 输出约 1.0
```

### es_log10

计算以10为底的对数。

```csharp
function es_log10(x: double): double
```

**参数:**
- `x`: 要计算对数的数值

**返回值:**
- 以10为底的对数值

**示例:**
```csharp
var result = es_log10(100);
_print_float(result);  // 输出 2.0
```

### es_exp

计算e的x次幂。

```csharp
function es_exp(x: double): double
```

**参数:**
- `x`: 指数

**返回值:**
- e的x次幂

**示例:**
```csharp
var result = es_exp(1);
_print_float(result);  // 输出约 2.71828
```

### es_fabs

计算绝对值。

```csharp
function es_fabs(x: double): double
```

**参数:**
- `x`: 要计算绝对值的数值

**返回值:**
- 绝对值

**示例:**
```csharp
var result = es_fabs(-3.14);
_print_float(result);  // 输出 3.14
```

### es_abs

计算整数的绝对值。

```csharp
function es_abs(x: int): int
```

**参数:**
- `x`: 要计算绝对值的整数

**返回值:**
- 绝对值

**示例:**
```csharp
var result = es_abs(-42);
_print_number(result);  // 输出 42
```

## 时间与日期

### es_time

获取当前时间（自1970年1月1日以来的秒数）。

```csharp
function es_time(): double
```

**返回值:**
- 当前时间（秒）

**示例:**
```csharp
var current = es_time();
_print_float(current);
```

### es_sleep

使当前线程休眠指定秒数。

```csharp
function es_sleep(seconds: int): void
```

**参数:**
- `seconds`: 休眠的秒数

**示例:**
```csharp
_print_string("Going to sleep for 2 seconds...");
es_sleep(2);
_print_string("Awake!");
```

### es_date

获取当前日期字符串。

```csharp
function es_date(): string
```

**返回值:**
- 当前日期字符串（需要释放内存）

**示例:**
```csharp
var date = es_date();
_print_string(date);
es_free(date);
```

### es_time_format

格式化时间字符串。

```csharp
function es_time_format(format: string): string
```

**参数:**
- `format`: 时间格式字符串

**返回值:**
- 格式化后的时间字符串（需要释放内存）

**示例:**
```csharp
var time = es_time_format("%Y-%m-%d %H:%M:%S");
_print_string(time);
es_free(time);
```

### timer_start

启动计时器。

```csharp
function timer_start(): double
```

**返回值:**
- 计时器ID

**示例:**
```csharp
var timer = timer_start();
// 执行一些操作
var elapsed = timer_elapsed();
_print_string("Elapsed time: ");
_print_float(elapsed);
_print_string(" seconds");
```

### timer_elapsed

获取自计时器启动以来经过的时间。

```csharp
function timer_elapsed(): double
```

**返回值:**
- 经过的时间（秒）

**示例:**
```csharp
var timer = timer_start();
es_sleep(1);
var elapsed = timer_elapsed();
_print_float(elapsed);  // 输出约 1.0
```

## 文件操作

### es_fopen

打开文件。

```csharp
function es_fopen(filename: string, mode: string): ES_FILE
```

**参数:**
- `filename`: 文件名
- `mode`: 打开模式（"r"读取，"w"写入，"a"追加等）

**返回值:**
- 文件指针，如果打开失败则返回null

**示例:**
```csharp
var file = es_fopen("test.txt", "w");
if (file != null) {
    es_fwrite("Hello, World!", 1, 13, file);
    es_fclose(file);
}
```

### es_fclose

关闭文件。

```csharp
function es_fclose(file: ES_FILE): int
```

**参数:**
- `file`: 要关闭的文件指针

**返回值:**
- 如果成功关闭返回0，否则返回非零值

**示例:**
```csharp
var file = es_fopen("test.txt", "r");
if (file != null) {
    // 读取文件
    es_fclose(file);
}
```

### es_fread

从文件读取数据。

```csharp
function es_fread(buffer: void*, size: size_t, count: size_t, file: ES_FILE): size_t
```

**参数:**
- `buffer`: 存储读取数据的缓冲区
- `size`: 每个元素的大小（字节）
- `count`: 要读取的元素数量
- `file`: 文件指针

**返回值:**
- 实际读取的元素数量

**示例:**
```csharp
var buffer = es_malloc(100);
var file = es_fopen("test.txt", "r");
if (file != null) {
    var bytesRead = es_fread(buffer, 1, 99, file);
    buffer[bytesRead] = '\0';
    _print_string(buffer);
    es_fclose(file);
}
es_free(buffer);
```

### es_fwrite

向文件写入数据。

```csharp
function es_fwrite(buffer: void*, size: size_t, count: size_t, file: ES_FILE): size_t
```

**参数:**
- `buffer`: 包含要写入数据的缓冲区
- `size`: 每个元素的大小（字节）
- `count`: 要写入的元素数量
- `file`: 文件指针

**返回值:**
- 实际写入的元素数量

**示例:**
```csharp
var text = "Hello, World!";
var file = es_fopen("test.txt", "w");
if (file != null) {
    es_fwrite(text, 1, es_strlen(text), file);
    es_fclose(file);
}
```

### es_fseek

设置文件位置指针。

```csharp
function es_fseek(file: ES_FILE, offset: long, origin: int): int
```

**参数:**
- `file`: 文件指针
- `offset`: 偏移量
- `origin`: 起始位置（0=文件开始，1=当前位置，2=文件结尾）

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var file = es_fopen("test.txt", "r");
if (file != null) {
    es_fseek(file, 0, 2);  // 移动到文件末尾
    var size = es_ftell(file);
    _print_string("File size: ");
    _print_number(size);
    es_fclose(file);
}
```

### es_ftell

获取当前文件位置。

```csharp
function es_ftell(file: ES_FILE): long
```

**参数:**
- `file`: 文件指针

**返回值:**
- 当前文件位置（从文件开始的字节数）

**示例:**
```csharp
var file = es_fopen("test.txt", "r");
if (file != null) {
    es_fseek(file, 0, 2);  // 移动到文件末尾
    var size = es_ftell(file);
    _print_string("File size: ");
    _print_number(size);
    es_fclose(file);
}
```

### es_remove

删除文件。

```csharp
function es_remove(filename: string): int
```

**参数:**
- `filename`: 要删除的文件名

**返回值:**
- 如果成功删除返回0，否则返回非零值

**示例:**
```csharp
var result = es_remove("temp.txt");
if (result == 0) {
    _print_string("File deleted successfully");
} else {
    _print_string("Failed to delete file");
}
```

## 数组操作

### array_create

创建数组。

```csharp
function array_create(element_size: size_t, initial_capacity: size_t): ES_Array*
```

**参数:**
- `element_size`: 每个元素的大小（字节）
- `initial_capacity`: 初始容量

**返回值:**
- 新创建的数组指针，如果创建失败则返回null

**示例:**
```csharp
var arr = array_create(4, 10);  // 创建整型数组，初始容量为10
if (arr != null) {
    // 使用数组
    array_ES_FREE(arr);
}
```

### array_ES_FREE

释放数组内存。

```csharp
function array_ES_FREE(array: ES_Array*): void
```

**参数:**
- `array`: 要释放的数组指针

**示例:**
```csharp
var arr = array_create(4, 10);
// 使用数组
array_ES_FREE(arr);
```

### array_size

获取数组当前大小。

```csharp
function array_size(array: ES_Array*): size_t
```

**参数:**
- `array`: 数组指针

**返回值:**
- 数组中元素的数量

**示例:**
```csharp
var arr = array_create(4, 10);
var size = array_size(arr);
_print_number(size);  // 输出 0
array_ES_FREE(arr);
```

### array_append

向数组添加元素。

```csharp
function array_append(array: ES_Array*, element: void*): int
```

**参数:**
- `array`: 数组指针
- `element`: 要添加的元素指针

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var arr = array_create(4, 10);
var value = 42;
array_append(arr, &value);
var size = array_size(arr);
_print_number(size);  // 输出 1
array_ES_FREE(arr);
```

### array_get

获取数组中指定索引的元素。

```csharp
function array_get(array: ES_Array*, index: size_t): void*
```

**参数:**
- `array`: 数组指针
- `index`: 元素索引

**返回值:**
- 指向元素的指针，如果索引无效则返回null

**示例:**
```csharp
var arr = array_create(4, 10);
var value = 42;
array_append(arr, &value);
var ptr = array_get(arr, 0);
if (ptr != null) {
    var num = *(int*)ptr;
    _print_number(num);  // 输出 42
}
array_ES_FREE(arr);
```

### array_set

设置数组中指定索引的元素。

```csharp
function array_set(array: ES_Array*, index: size_t, element: void*): int
```

**参数:**
- `array`: 数组指针
- `index`: 元素索引
- `element`: 要设置的元素指针

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var arr = array_create(4, 10);
var value1 = 42;
var value2 = 100;
array_append(arr, &value1);
array_set(arr, 0, &value2);
var ptr = array_get(arr, 0);
if (ptr != null) {
    var num = *(int*)ptr;
    _print_number(num);  // 输出 100
}
array_ES_FREE(arr);
```

## 哈希表操作

### hashmap_create

创建哈希表。

```csharp
function hashmap_create(initial_capacity: size_t): ES_HashMap*
```

**参数:**
- `initial_capacity`: 初始容量

**返回值:**
- 新创建的哈希表指针，如果创建失败则返回null

**示例:**
```csharp
var map = hashmap_create(10);
if (map != null) {
    // 使用哈希表
    hashmap_ES_FREE(map);
}
```

### hashmap_ES_FREE

释放哈希表内存。

```csharp
function hashmap_ES_FREE(map: ES_HashMap*): void
```

**参数:**
- `map`: 要释放的哈希表指针

**示例:**
```csharp
var map = hashmap_create(10);
// 使用哈希表
hashmap_ES_FREE(map);
```

### hashmap_put

向哈希表添加键值对。

```csharp
function hashmap_put(map: ES_HashMap*, key: void*, key_type: ES_HashMapType, value: void*, value_type: ES_HashMapType): int
```

**参数:**
- `map`: 哈希表指针
- `key`: 键指针
- `key_type`: 键类型
- `value`: 值指针
- `value_type`: 值类型

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var map = hashmap_create(10);
var key = "name";
var value = "E#";
hashmap_put(map, &key, ES_HASHMAP_TYPE_STRING, &value, ES_HASHMAP_TYPE_STRING);
hashmap_ES_FREE(map);
```

### hashmap_get

从哈希表获取值。

```csharp
function hashmap_get(map: ES_HashMap*, key: void*, key_type: ES_HashMapType, value: void**, out_value_type: ES_HashMapType*): int
```

**参数:**
- `map`: 哈希表指针
- `key`: 键指针
- `key_type`: 键类型
- `value`: 存储值的指针的指针
- `out_value_type`: 存储值类型的指针

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var map = hashmap_create(10);
var key = "name";
var value = "E#";
hashmap_put(map, &key, ES_HASHMAP_TYPE_STRING, &value, ES_HASHMAP_TYPE_STRING);

var retrieved_value;
var value_type;
if (hashmap_get(map, &key, ES_HASHMAP_TYPE_STRING, &retrieved_value, &value_type) == 0) {
    var str = *(string*)retrieved_value;
    _print_string(str);  // 输出 "E#"
}
hashmap_ES_FREE(map);
```

### hashmap_remove

从哈希表移除键值对。

```csharp
function hashmap_remove(map: ES_HashMap*, key: void*, key_type: ES_HashMapType): int
```

**参数:**
- `map`: 哈希表指针
- `key`: 键指针
- `key_type`: 键类型

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var map = hashmap_create(10);
var key = "name";
var value = "E#";
hashmap_put(map, &key, ES_HASHMAP_TYPE_STRING, &value, ES_HASHMAP_TYPE_STRING);
hashmap_remove(map, &key, ES_HASHMAP_TYPE_STRING);
hashmap_ES_FREE(map);
```

## 网络编程

### es_socket

创建套接字。

```csharp
function es_socket(domain: int, type: int, protocol: int): ES_Socket
```

**参数:**
- `domain`: 地址族（如AF_INET）
- `type`: 套接字类型（如SOCK_STREAM）
- `protocol`: 协议类型（通常为0）

**返回值:**
- 套接字描述符，如果创建失败则返回无效套接字

**示例:**
```csharp
var sock = es_socket(2, 1, 0);  // AF_INET, SOCK_STREAM
if (sock != null) {
    // 使用套接字
    es_close_socket(sock);
}
```

### es_connect

连接到远程服务器。

```csharp
function es_connect(socket: ES_Socket, address: string, port: int): int
```

**参数:**
- `socket`: 套接字描述符
- `address`: 服务器地址
- `port`: 服务器端口

**返回值:**
- 如果成功连接返回0，否则返回非零值

**示例:**
```csharp
var sock = es_socket(2, 1, 0);
if (es_connect(sock, "example.com", 80) == 0) {
    _print_string("Connected to server");
    es_close_socket(sock);
} else {
    _print_string("Connection failed");
}
```

### es_send

发送数据。

```csharp
function es_send(socket: ES_Socket, data: string, length: size_t): int
```

**参数:**
- `socket`: 套接字描述符
- `data`: 要发送的数据
- `length`: 数据长度

**返回值:**
- 实际发送的字节数，如果出错返回-1

**示例:**
```csharp
var sock = es_socket(2, 1, 0);
if (es_connect(sock, "example.com", 80) == 0) {
    var request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    es_send(sock, request, es_strlen(request));
    es_close_socket(sock);
}
```

### es_recv

接收数据。

```csharp
function es_recv(socket: ES_Socket, buffer: string, length: size_t): int
```

**参数:**
- `socket`: 套接字描述符
- `buffer`: 存储接收数据的缓冲区
- `length`: 缓冲区大小

**返回值:**
- 实际接收的字节数，如果出错返回-1，如果连接关闭返回0

**示例:**
```csharp
var sock = es_socket(2, 1, 0);
if (es_connect(sock, "example.com", 80) == 0) {
    var request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    es_send(sock, request, es_strlen(request));
    
    var buffer = es_malloc(1024);
    var bytesRead = es_recv(sock, buffer, 1023);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        _print_string(buffer);
    }
    es_free(buffer);
    es_close_socket(sock);
}
```

## 多线程

### es_thread_create_func

创建线程。

```csharp
function es_thread_create_func(func: function(void*), arg: void*): ES_Thread
```

**参数:**
- `func`: 线程函数指针
- `arg`: 传递给线程函数的参数

**返回值:**
- 线程句柄，如果创建失败则返回null

**示例:**
```csharp
function ThreadFunction(arg: void*): void {
    _print_string("Thread is running");
}

var thread = es_thread_create_func(ThreadFunction, null);
if (thread != null) {
    es_thread_join_func(thread);
}
```

### es_thread_join_func

等待线程结束。

```csharp
function es_thread_join_func(thread: ES_Thread): int
```

**参数:**
- `thread`: 线程句柄

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
function ThreadFunction(arg: void*): void {
    _print_string("Thread is running");
}

var thread = es_thread_create_func(ThreadFunction, null);
if (thread != null) {
    es_thread_join_func(thread);
    _print_string("Thread has finished");
}
```

### es_mutex_create_func

创建互斥锁。

```csharp
function es_mutex_create_func(): ES_Mutex
```

**返回值:**
- 互斥锁句柄，如果创建失败则返回null

**示例:**
```csharp
var mutex = es_mutex_create_func();
if (mutex != null) {
    es_mutex_lock_func(mutex);
    // 临界区代码
    es_mutex_unlock_func(mutex);
    es_mutex_free_func(mutex);
}
```

### es_mutex_lock_func

锁定互斥锁。

```csharp
function es_mutex_lock_func(mutex: ES_Mutex): int
```

**参数:**
- `mutex`: 互斥锁句柄

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var mutex = es_mutex_create_func();
if (mutex != null) {
    es_mutex_lock_func(mutex);
    // 临界区代码
    es_mutex_unlock_func(mutex);
    es_mutex_free_func(mutex);
}
```

### es_mutex_unlock_func

解锁互斥锁。

```csharp
function es_mutex_unlock_func(mutex: ES_Mutex): int
```

**参数:**
- `mutex`: 互斥锁句柄

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```csharp
var mutex = es_mutex_create_func();
if (mutex != null) {
    es_mutex_lock_func(mutex);
    // 临界区代码
    es_mutex_unlock_func(mutex);
    es_mutex_free_func(mutex);
}
```

## 图形界面

### es_window_create

创建窗口。

```csharp
function es_window_create(width: int, height: int, title: string): ES_Window
```

**参数:**
- `width`: 窗口宽度
- `height`: 窗口高度
- `title`: 窗口标题

**返回值:**
- 窗口句柄，如果创建失败则返回null

**示例:**
```csharp
var window = es_window_create(800, 600, "My Window");
if (window != null) {
    es_window_show(window);
    while (es_window_is_open(window)) {
        // 处理窗口事件
        es_window_update(window);
    }
}
```

### es_window_show

显示窗口。

```csharp
function es_window_show(window: ES_Window): void
```

**参数:**
- `window`: 窗口句柄

**示例:**
```csharp
var window = es_window_create(800, 600, "My Window");
if (window != null) {
    es_window_show(window);
    // 窗口事件循环
}
```

### es_window_clear

清除窗口内容。

```csharp
function es_window_clear(window: ES_Window, color: ES_Color): void
```

**参数:**
- `window`: 窗口句柄
- `color`: 清除颜色

**示例:**
```csharp
var window = es_window_create(800, 600, "My Window");
if (window != null) {
    es_window_show(window);
    var black = {0, 0, 0, 255};
    es_window_clear(window, black);
    // 窗口事件循环
}
```

### es_draw_rect

绘制矩形。

```csharp
function es_draw_rect(window: ES_Window, rect: ES_Rect, color: ES_Color): void
```

**参数:**
- `window`: 窗口句柄
- `rect`: 矩形位置和大小
- `color`: 矩形颜色

**示例:**
```csharp
var window = es_window_create(800, 600, "My Window");
if (window != null) {
    es_window_show(window);
    var black = {0, 0, 0, 255};
    var red = {255, 0, 0, 255};
    var rect = {100, 100, 200, 150};
    es_window_clear(window, black);
    es_draw_rect(window, rect, red);
    // 窗口事件循环
}
```

### es_draw_circle

绘制圆形。

```csharp
function es_draw_circle(window: ES_Window, center: ES_Point, radius: int, color: ES_Color): void
```

**参数:**
- `window`: 窗口句柄
- `center`: 圆心位置
- `radius`: 圆的半径
- `color`: 圆的颜色

**示例:**
```csharp
var window = es_window_create(800, 600, "My Window");
if (window != null) {
    es_window_show(window);
    var black = {0, 0, 0, 255};
    var blue = {0, 0, 255, 255};
    var center = {400, 300};
    es_window_clear(window, black);
    es_draw_circle(window, center, 50, blue);
    // 窗口事件循环
}
```

## 错误处理

### es_error

报告错误。

```csharp
function es_error(code: ES_ErrorCode): void
```

**参数:**
- `code`: 错误代码

**示例:**
```csharp
var file = es_fopen("nonexistent.txt", "r");
if (file == null) {
    es_error(ES_ERR_FILE_NOT_FOUND);
}
```

### es_strerror

获取错误代码对应的错误消息。

```csharp
function es_strerror(code: ES_ErrorCode): string
```

**参数:**
- `code`: 错误代码

**返回值:**
- 错误消息字符串

**示例:**
```csharp
var file = es_fopen("nonexistent.txt", "r");
if (file == null) {
    var msg = es_strerror(ES_ERR_FILE_NOT_FOUND);
    _print_string(msg);
}
```

### es_exit

退出程序。

```csharp
function es_exit(code: int): void
```

**参数:**
- `code`: 退出代码

**示例:**
```csharp
if (error_occurred) {
    es_exit(1);
} else {
    es_exit(0);
}
```

---

*本API参考文档涵盖了E#运行时库的主要函数和类型。更多详细信息请参考语言规范和开发者指南。*