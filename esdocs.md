E# 语言完整文档总结
========================

## 语言概述
E# 是一种语法类似C#的低级汇编语言，设计用于系统编程和底层开发。它结合了C#的高级语法糖和x86/x64汇编的底层控制能力，同时提供完整的内存安全保护。

### 主要特点
- **简洁语法**：类似C#的高级语法，易于学习
- **底层控制**：支持x86/x64汇编指令和内联汇编
- **内存安全**：边界检查、空指针保护、自动内存管理
- **跨平台**：支持Linux、Windows、未来支持ARM64
- **丰富标准库**：内存管理、字符串操作、文件IO
- **现代特性**：泛型、Lambda、异常处理、并发

### 编程模式
1. **高级模式**：类似C#的面向对象编程
2. **低级模式**：直接汇编编程，系统级开发

## 程序结构

### 命名空间
```csharp
namespace 程序名称;
// 代码内容
```

### 类定义
```csharp
class Calculator {
    private int result;
    
    public Calculator() {
        result = 0;
    }
    
    public int add(int a, int b) {
        result = a + b;
        return result;
    }
}
```

### 主方法
```csharp
class Program {
    void Main() {
        Console.WriteLine("程序开始执行");
    }
}
```

## 数据类型

### 基本类型
- **整数类型**：int8, int16, int32, int64, uint8, uint16, uint32, uint64
- **浮点类型**：float32, float64
- **布尔类型**：bool
- **字符串类型**：string

### 高级类型
- **指针类型**：int32*, void*, 双重指针
- **数组类型**：固定大小数组、动态数组、多维数组

## 变量声明

### 整数变量
```csharp
int32 age = 25;
uint64 big_number = 9223372036854775807;
int8 small = -128;
uint16 medium = 65535;
```

### 字符串变量
```csharp
string name = "张三";
string message = "欢迎使用E#";
string empty = "";
string unicode = "你好 🌍";
```

### 指针变量
```csharp
int32* ptr = new int32(42);
uint64* big_ptr = new uint64(1000000);
void* generic = nullptr;
```

## 函数定义

### 基本函数
```csharp
int add(int a, int b) {
    return a + b;
}
```

### 汇编函数
```csharp
int multiply(int a, int b) asm {
    mov eax, [a]
    imul eax, [b]
    ret
}
```

### 模板函数
```csharp
template<T>
T max(T a, T b) {
    return (a > b) ? a : b;
}
```

### Lambda表达式
```csharp
auto add = (int x, int y) -> int { return x + y; };
auto is_even = (int x) -> bool { return x % 2 == 0; };
```

## 控制台输出

### WriteLine
```csharp
Console.WriteLine("Hello, World!");
Console.WriteLine("当前数字: " + count);
Console.WriteLine("结果: {0}", result);
```

### Write
```csharp
Console.Write("请输入: ");
Console.Write("进度: " + percent + "%");
```

### 格式化输出
```csharp
Console.WriteLine("姓名: {0}, 年龄: {1}, 分数: {2:F2}", name, age, score);
Console.WriteLine("十六进制: {0:X}", 255);  // FF
```

## 控制语句

### 条件语句
```csharp
// 基本if
if (age >= 18) {
    Console.WriteLine("成年人");
}

// if-else
if (score >= 60) {
    Console.WriteLine("及格");
} else {
    Console.WriteLine("不及格");
}

// 多重条件
if (grade >= 90) {
    Console.WriteLine("优秀");
} else if (grade >= 80) {
    Console.WriteLine("良好");
} else {
    Console.WriteLine("不及格");
}
```

### 循环语句
```csharp
// for循环
for (int i = 0; i < 10; i++) {
    Console.WriteLine("数字: " + i);
}

// while循环
int count = 0;
while (count < 5) {
    Console.WriteLine("计数: " + count);
    count++;
}

// do-while循环
do {
    Console.WriteLine("请选择: 1.继续 2.退出");
    choice = int.parse(Console.ReadLine());
} while (choice != 2);
```

## 内存安全

### 安全特性
- **边界检查数组访问**：自动防止缓冲区溢出
- **空指针保护**：运行时空指针检查
- **自动内存管理**：智能指针和垃圾回收
- **内存泄漏防护**：RAII和自动清理
- **悬垂指针防护**：所有权系统和生命周期检查

### 安全指针使用
```csharp
int32* ptr = new int32(42);
if (!ptr.is_null()) {
    int32 value = *ptr;
    *ptr = 100;
}
delete ptr;
```

### 智能指针
```csharp
unique_ptr<int32> smart_ptr = make_unique<int32>(42);
shared_ptr<string> shared_str = make_shared<string>(Hello");
```

## 高级特性

### 数组和集合
```csharp
// 安全数组
safe_array<int> arr = safe_array<int>();
arr.add(1);
arr.add(2);
int sum = arr.sum();

// 列表和字典
List<string> list = new List<string>();
Dictionary<string, int> dict = new Dictionary<string, int>();
```

### 结构体和类
```csharp
struct Point3D {
    float x, y, z;
    
    float distance_to(Point3D other) {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return sqrt(dx*dx + dy*dy + dz*dz);
    }
};
```

### 泛型和模板
```csharp
class List<T> {
    private T* data;
    private int size;
    private int capacity;
    
    public List() {
        capacity = 10;
        size = 0;
        data = new T[capacity];
    }
    
    public void add(T item) {
        if (size >= capacity) {
            resize();
        }
        data[size++] = item;
    }
};
```

### 异常处理
```csharp
try {
    int* data = new int[1000000];
    if (data == null) {
        throw MemoryAllocationException("内存分配失败");
    }
    int result = divide(10, user_input);
} catch (DivisionByZeroException e) {
    Console.WriteLine("错误：除数不能为零");
} catch (MemoryAllocationException e) {
    Console.WriteLine("错误：内存不足");
} finally {
    cleanup();
}
```

### 并发支持
```csharp
// 线程
Thread worker = create_thread(void() {
    for (int i = 0; i < 100; i++) {
        Console.WriteLine("工作中: " + i);
    }
});

// 互斥锁
mutex data_mutex;
void safe_increment() {
    lock(data_mutex) {
        shared_data++;
    }
}

// 原子操作
atomic<int> counter = 0;
void increment_counter() {
    counter.fetch_add(1);
}
```

## 内联汇编

### 基本汇编块
```csharp
asm {
    mov eax, 1
    add eax, ebx
    mov [result], eax
}
```

### Volatile汇编
```csharp
asm volatile {
    cli  // 禁用中断
    hlt  // 停机
}
```

## 示例程序

### Hello World
```csharp
// 高级C#风格
namespace HelloWorld;

class Program {
    void Main() {
        Console.WriteLine("Hello, World from E#!");
    }
}

// Linux汇编风格
section .data
    msg: db "Hello from E# assembly!", 0xA
    len: equ $ - msg

section .text
    global _start

void _start() asm {
    mov rax, 1          // sys_write
    mov rdi, 1          // stdout
    mov rsi, msg        // 消息
    mov rdx, len        // 长度
    syscall
    
    mov rax, 60         // sys_exit
    xor rdi, rdi        // 状态码0
    syscall
}
```

### 斐波那契数列
```csharp
int fibonacci_iterative(int n) {
    if (n <= 1) return n;
    
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}
```

## 标准库

### 内存管理
```csharp
// 内存分配
ptr malloc(uint64 size);
void free(ptr p);
ptr memset(ptr dest, int value, uint64 size);
ptr memcpy(ptr dest, const ptr src, uint64 size);

// 内存信息
uint64 get_total_memory();
uint64 get_free_memory();
MemoryStats get_memory_stats();
```

### 字符串操作
```csharp
// 基本操作
uint64 strlen(string s);
int strcmp(string s1, string s2);
void strcpy(string dest, string src);
string strcat(string dest, string src);

// 高级操作
string substring(string s, int start, int length);
int find(string haystack, string needle);
string replace(string s, string old, string new);
string to_upper(string s);
string to_lower(string s);
```

### 输入输出
```csharp
// 基本IO
void print(string msg);
string read_line();
int read_int();
double read_double();

// 文件IO
class File {
    static File open(string filename, string mode);
    void write(string content);
    string read_all();
    void close();
};
```

## 总结
E# 是一门结合了高级语言特性和底层汇编控制的现代编程语言，具有以下核心优势：

1. **语法友好**：类似C#的语法，易于学习和使用
2. **内存安全**：多层防护机制，防止常见的内存错误
3. **性能优越**：支持内联汇编和底层优化
4. **功能丰富**：支持现代编程特性如泛型、异常处理、并发等
5. **跨平台**：支持多种操作系统和架构

适用于系统编程、嵌入式开发、性能敏感应用等场景。