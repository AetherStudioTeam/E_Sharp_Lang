E# 语言完整文档 2.0
========================

## 语言概述
E# 是一种语法类似C#的低级汇编语言，设计用于系统编程和底层开发。它结合了C#的高级语法糖和x86/x64汇编的底层控制能力，同时提供完整的内存安全保护。

### 主要特点
- **简洁语法**：类似C#的高级语法，易于学习
- **底层控制**：支持x86/x64汇编指令和内联汇编
- **内存安全**：边界检查、空指针保护、自动内存管理
- **跨平台**：支持Linux、Windows、未来支持ARM64
- **丰富标准库**：内存管理、字符串操作、文件IO
- **现代特性**：泛型、Lambda、异常处理、并发、静态成员

### 编程模式
1. **高级模式**：类似C#的面向对象编程
2. **低级模式**：直接汇编编程，系统级开发

## 程序结构

### 命名空间
```esharp
namespace 程序名称;
// 代码内容
```

### 类定义
```esharp
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

### 静态成员
E# 支持两种静态成员语法：

#### 1. C#风格静态成员（推荐）
```esharp
class TestClass {
    // 静态变量
    public static var static_var = 42;
    
    // 静态函数
    public static int static_function(int x, int y) {
        return x + y + static_var;
    }
}

// 使用静态成员
int main() {
    var result = TestClass.static_function(10, 20);
    var value = TestClass.static_var;
    return 0;
}
```

#### 2. 传统静态成员语法
```esharp
class TestClass {
    // 静态变量声明
    static var static_var = 42;
    
    // 静态函数声明
    static int static_function() {
        print("Static function called");
        return static_var;
    }
}
```

#### 3. 带访问修饰符的静态成员
```esharp
class TestClass {
    public: static var public_static_var = 100;
    private: static var private_static_var = 200;
    protected: static var protected_static_var = 300;
    
    public: static int public_static_method() {
        return public_static_var;
    }
}
```

### 主方法
```esharp
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
```esharp
int32 age = 25;
uint64 big_number = 9223372036854775807;
int8 small = -128;
uint16 medium = 65535;
```

### 字符串变量
```esharp
string name = "张三（李四，王五）";
string message = "欢迎使用E#";
string empty = "";
string unicode = "你好";
```

### 指针变量
```esharp
int32* ptr = new int32(42);
uint64* big_ptr = new uint64(1000000);
void* generic = nullptr;
```

## 函数定义

### 基本函数
```esharp
int add(int a, int b) {
    return a + b;
}
```

### 静态函数
```esharp
// 类内静态函数
class MathUtils {
    public static int multiply(int a, int b) {
        return a * b;
    }
    
    public static double divide(double a, double b) {
        if (b == 0) {
            throw "Division by zero";
        }
        return a / b;
    }
}

// 全局静态函数（命名空间内）
namespace Utils {
    static string format_number(int num) {
        return "Number: " + num;
    }
}
```

### 汇编函数
```esharp
int multiply(int a, int b) asm {
    mov eax, [a]
    imul eax, [b]
    ret
}
```

### 模板函数
```esharp
template<T>
T max(T a, T b) {
    return (a > b) ? a : b;
}
```

### Lambda表达式
```esharp
auto add = (int x, int y) -> int { return x + y; };
auto is_even = (int x) -> bool { return x % 2 == 0; };
```

## 控制台输出

### WriteLine
```esharp
Console.WriteLine("Hello, World!");
Console.WriteLine("当前数字: " + count);
Console.WriteLine("结果: {0}", result);
```

### Write
```esharp
Console.Write("请输入: ");
Console.Write("进度: " + percent + "%");
```

### 格式化输出
```esharp
Console.WriteLine("姓名: {0}, 年龄: {1}, 分数: {2:F2}", name, age, score);
Console.WriteLine("十六进制: {0:X}", 255);  // FF
```

### 不习惯? 我们还有print!
```esharp
print("Hello World!")
```

## 控制语句

### 条件语句
```esharp
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
```esharp
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

## 静态成员高级用法

### 静态成员访问规则
```esharp
class Example {
    // 静态变量可以在静态函数和实例函数中访问
    static var counter = 0;
    
    // 实例变量
    var instance_var = 0;
    
    // 静态函数只能访问静态成员
    static int increment_counter() {
        counter++;  // ✅ 正确：访问静态变量
        // instance_var++;  // ❌ 错误：不能在静态函数中访问实例变量
        return counter;
    }
    
    // 实例函数可以访问静态和实例成员
    int use_both() {
        counter++;        // ✅ 正确：可以访问静态变量
        instance_var++;   // ✅ 正确：可以访问实例变量
        return counter + instance_var;
    }
}
```

### 静态成员初始化
```esharp
class Configuration {
    // 静态成员在类加载时初始化
    public static var max_connections = 100;
    public static var timeout_seconds = 30;
    public static var app_name = "MyApplication";
    
    // 静态函数用于配置管理
    public static void set_max_connections(int max) {
        max_connections = max;
    }
    
    public static string get_config_info() {
        return app_name + " - Max: " + max_connections + ", Timeout: " + timeout_seconds;
    }
}
```

### 工具类模式
```esharp
class StringUtils {
    // 纯静态工具类
    public static bool is_empty(string str) {
        return str == null || str.length == 0;
    }
    
    public static string reverse(string str) {
        if (is_empty(str)) return "";
        
        string result = "";
        for (int i = str.length - 1; i >= 0; i--) {
            result += str[i];
        }
        return result;
    }
    
    public static int count_occurrences(string str, string substring) {
        if (is_empty(str) || is_empty(substring)) return 0;
        
        int count = 0;
        int index = 0;
        while ((index = str.index_of(substring, index)) != -1) {
            count++;
            index += substring.length;
        }
        return count;
    }
}

// 使用工具类
int main() {
    var text = "Hello World";
    var reversed = StringUtils.reverse(text);
    var is_empty = StringUtils.is_empty(text);
    var count = StringUtils.count_occurrences(text, "l");
    
    Console.WriteLine("Original: " + text);
    Console.WriteLine("Reversed: " + reversed);
    Console.WriteLine("Is empty: " + is_empty);
    Console.WriteLine("Count of 'l': " + count);
    return 0;
}
```

## 内存安全

### 安全特性
- **边界检查数组访问**：自动防止缓冲区溢出
- **空指针保护**：运行时空指针检查
- **自动内存管理**：智能指针和垃圾回收
- **内存泄漏防护**：RAII和自动清理
- **悬垂指针防护**：所有权系统和生命周期检查

### 安全指针使用
```esharp
int32* ptr = new int32(42);
if (!ptr.is_null()) {
    int32 value = *ptr;
    *ptr = 100;
}
delete ptr;
```

### 智能指针
```esharp
unique_ptr<int32> smart_ptr = make_unique<int32>(42);
shared_ptr<string> shared_str = make_shared<string>(Hello");
```

## 高级特性

### 数组和集合
```esharp
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
```esharp
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
```esharp
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
```esharp
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
```esharp
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
```esharp
asm {
    mov eax, 1
    add eax, ebx
    mov [result], eax
}
```

### Volatile汇编
```esharp
asm volatile {
    cli  // 禁用中断
    hlt  // 停机
}
```

## 示例程序

### Hello World
```esharp
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

### 静态成员完整示例
```esharp
// 数学工具类
class MathUtils {
    // 数学常量
    public static var PI = 3.14159265359;
    public static var E = 2.71828182846;
    
    // 静态工具函数
    public static double abs(double x) {
        return x < 0 ? -x : x;
    }
    
    public static double power(double base, int exp) {
        if (exp == 0) return 1;
        double result = base;
        for (int i = 1; i < exp; i++) {
            result *= base;
        }
        return result;
    }
    
    public static int factorial(int n) {
        if (n <= 1) return 1;
        return n * factorial(n - 1);
    }
}

// 配置管理器
class ConfigManager {
    private static var config_data = new Dictionary<string, string>();
    
    public static void set_config(string key, string value) {
        config_data[key] = value;
    }
    
    public static string get_config(string key) {
        if (config_data.contains_key(key)) {
            return config_data[key];
        }
        return "";
    }
    
    public static void load_defaults() {
        set_config("app_name", "E# Application");
        set_config("version", "2.0");
        set_config("debug_mode", "true");
    }
}

int main() {
    // 使用数学工具类
    Console.WriteLine("PI = " + MathUtils.PI);
    Console.WriteLine("5! = " + MathUtils.factorial(5));
    Console.WriteLine("2^8 = " + MathUtils.power(2, 8));
    Console.WriteLine("|-42| = " + MathUtils.abs(-42));
    
    // 使用配置管理器
    ConfigManager.load_defaults();
    Console.WriteLine("App: " + ConfigManager.get_config("app_name"));
    Console.WriteLine("Version: " + ConfigManager.get_config("version"));
    
    return 0;
}
```

### 斐波那契数列
```esharp
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
```esharp
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
```esharp
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
```esharp
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

## 版本更新日志

### E# 2.0 新特性
1. **静态成员支持**
   - C#风格静态成员语法：`ClassName.static_member`
   - 传统静态成员语法：`static function` 和 `static var`
   - 带访问修饰符的静态成员：`public: static`
   - 静态函数调用调试信息优化

2. **改进的调试信息**
   - 静态成员访问显示详细信息
   - 函数调用类型区分（普通函数 vs 静态函数）
   - 增强的编译时错误提示

3. **性能优化**
   - 静态成员访问优化
   - IR代码生成效率提升
   - 内存管理改进


---
*本文档基于E#编译器最新版本编写，包含了完整的静态成员功能文档。*