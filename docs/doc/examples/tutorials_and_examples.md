# E# 示例和教程

## 目录

1. [入门示例](#入门示例)
2. [基础教程](#基础教程)
3. [进阶教程](#进阶教程)
4. [实战项目](#实战项目)
5. [常见问题解决方案](#常见问题解决方案)

## 入门示例

### Hello World

这是最简单的E#程序，展示了基本的程序结构和输出功能：

```csharp
function main(): void {
    print("Hello, World!");
}
```

编译和运行：
```bash
e_sharp hello.es
./hello
```

### 变量和基本类型

这个示例展示了如何声明和使用不同类型的变量：

```csharp
function main(): void {
    // 整数
    var age: int32 = 30;
    print("Age: " + age);
    
    // 浮点数
    var height: float64 = 1.75;
    print("Height: " + height);
    
    // 字符串
    var name: string = "Alice";
    print("Name: " + name);
    
    // 布尔值
    var isStudent: bool = true;
    print("Is student: " + isStudent);
    
    // 数组
    var numbers: int32[] = [1, 2, 3, 4, 5];
    print("Numbers: " + numbers[0] + ", " + numbers[1]);
}
```

### 基本运算

这个示例展示了各种基本运算：

```csharp
function main(): void {
    // 算术运算
    var a: int32 = 10;
    var b: int32 = 3;
    print("a + b = " + (a + b));  // 13
    print("a - b = " + (a - b));  // 7
    print("a * b = " + (a * b));  // 30
    print("a / b = " + (a / b));  // 3
    print("a % b = " + (a % b));  // 1
    
    // 比较运算
    print("a > b: " + (a > b));    // true
    print("a < b: " + (a < b));    // false
    print("a == b: " + (a == b));  // false
    
    // 逻辑运算
    var x: bool = true;
    var y: bool = false;
    print("x and y: " + (x and y));  // false
    print("x or y: " + (x or y));    // true
    print("not x: " + (not x));       // false
}
```

## 基础教程

### 教程1：控制流

这个教程展示了如何使用条件语句和循环：

```csharp
function main(): void {
    // if-else语句
    var score: int32 = 85;
    if (score >= 90) {
        print("Grade: A");
    } else if (score >= 80) {
        print("Grade: B");
    } else if (score >= 70) {
        print("Grade: C");
    } else if (score >= 60) {
        print("Grade: D");
    } else {
        print("Grade: F");
    }
    
    // switch语句
    var day: int32 = 3;
    switch (day) {
        case 1:
            print("Monday");
            break;
        case 2:
            print("Tuesday");
            break;
        case 3:
            print("Wednesday");
            break;
        case 4:
            print("Thursday");
            break;
        case 5:
            print("Friday");
            break;
        case 6:
            print("Saturday");
            break;
        case 7:
            print("Sunday");
            break;
        default:
            print("Invalid day");
            break;
    }
    
    // while循环
    var i: int32 = 1;
    while (i <= 5) {
        print("While loop: " + i);
        i = i + 1;
    }
    
    // for循环
    for (var j: int32 = 1; j <= 5; j = j + 1) {
        print("For loop: " + j);
    }
    
    // foreach循环
    var fruits: string[] = ["Apple", "Banana", "Cherry"];
    foreach (var fruit in fruits) {
        print("Fruit: " + fruit);
    }
}
```

### 教程2：函数

这个教程展示了如何定义和使用函数：

```csharp
// 简单函数
function Add(a: int32, b: int32): int32 {
    return a + b;
}

// 带有多个返回路径的函数
function Max(a: int32, b: int32): int32 {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

// 递归函数
function Factorial(n: int32): int32 {
    if (n <= 1) {
        return 1;
    } else {
        return n * Factorial(n - 1);
    }
}

// 引用参数
function Swap(ref a: int32, ref b: int32): void {
    var temp: int32 = a;
    a = b;
    b = temp;
}

function main(): void {
    // 调用简单函数
    var sum: int32 = Add(5, 3);
    print("5 + 3 = " + sum);
    
    // 调用Max函数
    var max: int32 = Max(10, 20);
    print("Max of 10 and 20: " + max);
    
    // 调用递归函数
    var fact: int32 = Factorial(5);
    print("5! = " + fact);
    
    // 调用引用参数函数
    var x: int32 = 10;
    var y: int32 = 20;
    print("Before swap: x = " + x + ", y = " + y);
    Swap(ref x, ref y);
    print("After swap: x = " + x + ", y = " + y);
}
```

### 教程3：数组操作

这个教程展示了如何使用数组：

```csharp
function main(): void {
    // 创建和初始化数组
    var numbers: int32[] = [1, 2, 3, 4, 5];
    var names: string[] = ["Alice", "Bob", "Charlie"];
    
    // 访问数组元素
    print("First number: " + numbers[0]);
    print("Last name: " + names[names.length - 1]);
    
    // 修改数组元素
    numbers[0] = 10;
    print("Modified first number: " + numbers[0]);
    
    // 遍历数组
    print("All numbers:");
    for (var i: int32 = 0; i < numbers.length; i = i + 1) {
        print("  " + numbers[i]);
    }
    
    // 使用foreach遍历数组
    print("All names:");
    foreach (var name in names) {
        print("  " + name);
    }
    
    // 数组操作函数
    print("Sum of numbers: " + Sum(numbers));
    print("Average of numbers: " + Average(numbers));
    print("Maximum number: " + Max(numbers));
    
    // 查找元素
    var index: int32 = Find(numbers, 3);
    if (index >= 0) {
        print("Found 3 at index: " + index);
    } else {
        print("3 not found in the array");
    }
}

// 计算数组元素和
function Sum(arr: int32[]): int32 {
    var total: int32 = 0;
    foreach (var num in arr) {
        total = total + num;
    }
    return total;
}

// 计算数组平均值
function Average(arr: int32[]): float64 {
    if (arr.length == 0) {
        return 0.0;
    }
    return Sum(arr) / (float64)arr.length;
}

// 查找数组中的最大值
function Max(arr: int32[]): int32 {
    if (arr.length == 0) {
        return 0;
    }
    
    var maxVal: int32 = arr[0];
    for (var i: int32 = 1; i < arr.length; i = i + 1) {
        if (arr[i] > maxVal) {
            maxVal = arr[i];
        }
    }
    return maxVal;
}

// 查找元素在数组中的索引
function Find(arr: int32[], target: int32): int32 {
    for (var i: int32 = 0; i < arr.length; i = i + 1) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;  // 未找到
}
```

## 进阶教程

### 教程4：面向对象编程

这个教程展示了如何使用类和对象：

```csharp
// 定义Person类
class Person {
    // 私有字段
    private var name: string;
    private var age: int32;
    
    // 构造函数
    function Person(name: string, age: int32) {
        this.name = name;
        this.age = age;
    }
    
    // 公共方法
    public function GetName(): string {
        return this.name;
    }
    
    public function GetAge(): int32 {
        return this.age;
    }
    
    public function SetAge(age: int32): void {
        if (age >= 0) {
            this.age = age;
        }
    }
    
    public function Introduce(): void {
        print("Hi, my name is " + this.name + " and I'm " + this.age + " years old.");
    }
    
    // 静态方法
    public static function CreateAdult(name: string): Person {
        return new Person(name, 18);
    }
}

// 定义Student类，继承自Person
class Student : Person {
    private var studentId: string;
    private var grades: float64[];
    
    function Student(name: string, age: int32, studentId: string): base(name, age) {
        this.studentId = studentId;
        this.grades = [];
    }
    
    public function GetStudentId(): string {
        return this.studentId;
    }
    
    public function AddGrade(grade: float64): void {
        this.grades[this.grades.length] = grade;
    }
    
    public function GetAverageGrade(): float64 {
        if (this.grades.length == 0) {
            return 0.0;
        }
        
        var total: float64 = 0.0;
        foreach (var grade in this.grades) {
            total = total + grade;
        }
        return total / (float64)this.grades.length;
    }
    
    public override function Introduce(): void {
        base.Introduce();
        print("My student ID is " + this.studentId + " and my average grade is " + this.GetAverageGrade());
    }
}

function main(): void {
    // 创建Person对象
    var person: Person = new Person("Alice", 30);
    person.Introduce();
    
    // 使用静态方法创建Person对象
    var adult: Person = Person.CreateAdult("Bob");
    adult.Introduce();
    
    // 创建Student对象
    var student: Student = new Student("Charlie", 20, "S12345");
    student.AddGrade(85.5);
    student.AddGrade(92.0);
    student.AddGrade(78.5);
    student.Introduce();
    
    // 访问继承的方法
    print("Student's name: " + student.GetName());
    print("Student's age: " + student.GetAge());
}
```

### 教程5：泛型编程

这个教程展示了如何使用泛型：

```csharp
// 泛型Box类
class Box<T> {
    private var value: T;
    
    function Box(value: T) {
        this.value = value;
    }
    
    public function GetValue(): T {
        return this.value;
    }
    
    public function SetValue(value: T): void {
        this.value = value;
    }
    
    public function ToString(): string {
        return "Box containing: " + this.value;
    }
}

// 泛型比较函数
function Compare<T>(a: T, b: T): int32 {
    if (a == b) {
        return 0;
    } else if (a > b) {
        return 1;
    } else {
        return -1;
    }
}

// 泛型交换函数
function Swap<T>(ref a: T, ref b: T): void {
    var temp: T = a;
    a = b;
    b = temp;
}

// 泛型查找函数
function FindInArray<T>(arr: T[], target: T): int32 {
    for (var i: int32 = 0; i < arr.length; i = i + 1) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;  // 未找到
}

function main(): void {
    // 使用泛型Box类
    var intBox: Box<int32> = new Box<int32>(42);
    var stringBox: Box<string> = new Box<string>("Hello");
    
    print(intBox.ToString());
    print(stringBox.ToString());
    
    intBox.SetValue(100);
    print(intBox.ToString());
    
    // 使用泛型比较函数
    var intResult: int32 = Compare<int32>(5, 10);
    print("Compare(5, 10): " + intResult);
    
    var stringResult: int32 = Compare<string>("Apple", "Banana");
    print("Compare(\"Apple\", \"Banana\"): " + stringResult);
    
    // 使用泛型交换函数
    var x: int32 = 10;
    var y: int32 = 20;
    print("Before swap: x = " + x + ", y = " + y);
    Swap<int32>(ref x, ref y);
    print("After swap: x = " + x + ", y = " + y);
    
    var s1: string = "Hello";
    var s2: string = "World";
    print("Before swap: s1 = " + s1 + ", s2 = " + s2);
    Swap<string>(ref s1, ref s2);
    print("After swap: s1 = " + s1 + ", s2 = " + s2);
    
    // 使用泛型查找函数
    var numbers: int32[] = [1, 2, 3, 4, 5];
    var words: string[] = ["Apple", "Banana", "Cherry"];
    
    var numIndex: int32 = FindInArray<int32>(numbers, 3);
    print("Index of 3 in numbers array: " + numIndex);
    
    var wordIndex: int32 = FindInArray<string>(words, "Banana");
    print("Index of \"Banana\" in words array: " + wordIndex);
}
```

### 教程6：异常处理

这个教程展示了如何使用异常处理：

```csharp
// 自定义异常类
class DivisionByZeroException : Exception {
    function DivisionByZeroException(): base("Division by zero") {
    }
}

class InvalidAgeException : Exception {
    function InvalidAgeException(age: int32): base("Invalid age: " + age) {
    }
}

// 可能抛出异常的函数
function Divide(a: float64, b: float64): float64 {
    if (b == 0.0) {
        throw new DivisionByZeroException();
    }
    return a / b;
}

function ProcessAge(age: int32): void {
    if (age < 0 || age > 150) {
        throw new InvalidAgeException(age);
    }
    print("Processing age: " + age);
}

function main(): void {
    // 基本异常处理
    try {
        var result: float64 = Divide(10.0, 0.0);
        print("Result: " + result);
    } catch (DivisionByZeroException e) {
        print("Error: " + e.Message);
    } catch (Exception e) {
        print("General error: " + e.Message);
    }
    
    // 嵌套异常处理
    try {
        try {
            ProcessAge(-5);
        } catch (InvalidAgeException e) {
            print("Invalid age error: " + e.Message);
            // 重新抛出异常
            throw e;
        }
    } catch (Exception e) {
        print("Caught rethrown exception: " + e.Message);
    }
    
    // finally块
    var file: any = null;  // 假设的文件对象
    try {
        // 打开文件
        file = "OpenFile";  // 模拟文件打开
        print("File opened");
        
        // 处理文件
        ProcessAge(25);
        
        // 可能抛出异常的操作
        var result: float64 = Divide(10.0, 2.0);
        print("Result: " + result);
    } catch (Exception e) {
        print("Error during file processing: " + e.Message);
    } finally {
        // 无论是否发生异常都会执行
        if (file != null) {
            // 关闭文件
            file = null;
            print("File closed");
        }
    }
    
    // 多个catch块
    try {
        var ages: int32[] = [25, -5, 200];
        foreach (var age in ages) {
            ProcessAge(age);
            var result: float64 = Divide(10.0, age);
            print("10 / " + age + " = " + result);
        }
    } catch (DivisionByZeroException e) {
        print("Division error: " + e.Message);
    } catch (InvalidAgeException e) {
        print("Age error: " + e.Message);
    } catch (Exception e) {
        print("Unexpected error: " + e.Message);
    }
}
```

## 实战项目

### 项目1：简单计算器

这个项目实现了一个支持基本运算的计算器：

```csharp
class Calculator {
    public function Add(a: float64, b: float64): float64 {
        return a + b;
    }
    
    public function Subtract(a: float64, b: float64): float64 {
        return a - b;
    }
    
    public function Multiply(a: float64, b: float64): float64 {
        return a * b;
    }
    
    public function Divide(a: float64, b: float64): float64 {
        if (b == 0.0) {
            throw new Exception("Division by zero");
        }
        return a / b;
    }
    
    public function Power(base: float64, exponent: float64): float64 {
        return pow(base, exponent);
    }
    
    public function SquareRoot(x: float64): float64 {
        if (x < 0.0) {
            throw new Exception("Cannot calculate square root of negative number");
        }
        return sqrt(x);
    }
}

function main(): void {
    var calc: Calculator = new Calculator();
    var a: float64 = 10.0;
    var b: float64 = 5.0;
    
    print("Calculator Demo:");
    print("a = " + a + ", b = " + b);
    
    try {
        print("a + b = " + calc.Add(a, b));
        print("a - b = " + calc.Subtract(a, b));
        print("a * b = " + calc.Multiply(a, b));
        print("a / b = " + calc.Divide(a, b));
        print("a ^ b = " + calc.Power(a, b));
        print("sqrt(a) = " + calc.SquareRoot(a));
        
        // 测试异常
        print("a / 0 = " + calc.Divide(a, 0.0));
    } catch (Exception e) {
        print("Error: " + e.Message);
    }
}
```

### 项目2：学生成绩管理系统

这个项目实现了一个简单的学生成绩管理系统：

```csharp
class Student {
    private var id: string;
    private var name: string;
    private var grades: float64[];
    
    function Student(id: string, name: string) {
        this.id = id;
        this.name = name;
        this.grades = [];
    }
    
    public function GetId(): string {
        return this.id;
    }
    
    public function GetName(): string {
        return this.name;
    }
    
    public function AddGrade(grade: float64): void {
        if (grade < 0.0 || grade > 100.0) {
            throw new Exception("Grade must be between 0 and 100");
        }
        this.grades[this.grades.length] = grade;
    }
    
    public function GetAverageGrade(): float64 {
        if (this.grades.length == 0) {
            return 0.0;
        }
        
        var total: float64 = 0.0;
        foreach (var grade in this.grades) {
            total = total + grade;
        }
        return total / (float64)this.grades.length;
    }
    
    public function GetGradeCount(): int32 {
        return this.grades.length;
    }
    
    public function GetHighestGrade(): float64 {
        if (this.grades.length == 0) {
            return 0.0;
        }
        
        var highest: float64 = this.grades[0];
        for (var i: int32 = 1; i < this.grades.length; i = i + 1) {
            if (this.grades[i] > highest) {
                highest = this.grades[i];
            }
        }
        return highest;
    }
    
    public function GetLowestGrade(): float64 {
        if (this.grades.length == 0) {
            return 0.0;
        }
        
        var lowest: float64 = this.grades[0];
        for (var i: int32 = 1; i < this.grades.length; i = i + 1) {
            if (this.grades[i] < lowest) {
                lowest = this.grades[i];
            }
        }
        return lowest;
    }
}

class GradeManager {
    private var students: Student[];
    
    function GradeManager() {
        this.students = [];
    }
    
    public function AddStudent(id: string, name: string): void {
        // 检查学生ID是否已存在
        foreach (var student in this.students) {
            if (student.GetId() == id) {
                throw new Exception("Student with ID " + id + " already exists");
            }
        }
        
        this.students[this.students.length] = new Student(id, name);
    }
    
    public function FindStudent(id: string): Student {
        foreach (var student in this.students) {
            if (student.GetId() == id) {
                return student;
            }
        }
        throw new Exception("Student with ID " + id + " not found");
    }
    
    public function AddGrade(id: string, grade: float64): void {
        var student: Student = FindStudent(id);
        student.AddGrade(grade);
    }
    
    public function GetStudentCount(): int32 {
        return this.students.length;
    }
    
    public function PrintStudentInfo(id: string): void {
        try {
            var student: Student = FindStudent(id);
            print("Student ID: " + student.GetName());
            print("Name: " + student.GetName());
            print("Number of grades: " + student.GetGradeCount());
            print("Average grade: " + student.GetAverageGrade());
            print("Highest grade: " + student.GetHighestGrade());
            print("Lowest grade: " + student.GetLowestGrade());
        } catch (Exception e) {
            print("Error: " + e.Message);
        }
    }
    
    public function PrintAllStudents(): void {
        print("All Students:");
        print("----------------------------");
        foreach (var student in this.students) {
            print("ID: " + student.GetId() + ", Name: " + student.GetName() + 
                  ", Average: " + student.GetAverageGrade());
        }
        print("----------------------------");
    }
    
    public function GetClassAverage(): float64 {
        if (this.students.length == 0) {
            return 0.0;
        }
        
        var total: float64 = 0.0;
        var gradeCount: int32 = 0;
        
        foreach (var student in this.students) {
            if (student.GetGradeCount() > 0) {
                total = total + student.GetAverageGrade();
                gradeCount = gradeCount + 1;
            }
        }
        
        if (gradeCount == 0) {
            return 0.0;
        }
        
        return total / (float64)gradeCount;
    }
}

function main(): void {
    var manager: GradeManager = new GradeManager();
    
    try {
        // 添加学生
        manager.AddStudent("S001", "Alice");
        manager.AddStudent("S002", "Bob");
        manager.AddStudent("S003", "Charlie");
        
        // 添加成绩
        manager.AddGrade("S001", 85.5);
        manager.AddGrade("S001", 92.0);
        manager.AddGrade("S001", 78.5);
        
        manager.AddGrade("S002", 90.0);
        manager.AddGrade("S002", 88.5);
        
        manager.AddGrade("S003", 75.0);
        manager.AddGrade("S003", 82.5);
        manager.AddGrade("S003", 79.0);
        manager.AddGrade("S003", 91.5);
        
        // 打印所有学生信息
        manager.PrintAllStudents();
        
        // 打印班级平均分
        print("Class average: " + manager.GetClassAverage());
        
        // 打印特定学生信息
        print("\nDetailed information for student S001:");
        manager.PrintStudentInfo("S001");
        
        // 测试异常情况
        print("\nTesting error cases:");
        manager.AddStudent("S001", "Duplicate ID");  // 应该抛出异常
    } catch (Exception e) {
        print("Error: " + e.Message);
    }
}
```

### 项目3：简单的文本冒险游戏

这个项目实现了一个简单的文本冒险游戏：

```csharp
class Room {
    private var name: string;
    private var description: string;
    private var exits: string[];
    private var exitDirections: string[];
    
    function Room(name: string, description: string) {
        this.name = name;
        this.description = description;
        this.exits = [];
        this.exitDirections = [];
    }
    
    public function GetName(): string {
        return this.name;
    }
    
    public function GetDescription(): string {
        return this.description;
    }
    
    public function AddExit(direction: string, roomName: string): void {
        this.exitDirections[this.exitDirections.length] = direction;
        this.exits[this.exits.length] = roomName;
    }
    
    public function GetExit(direction: string): string {
        for (var i: int32 = 0; i < this.exitDirections.length; i = i + 1) {
            if (this.exitDirections[i] == direction) {
                return this.exits[i];
            }
        }
        return "";
    }
    
    public function GetExitDirections(): string[] {
        return this.exitDirections;
    }
}

class Game {
    private var rooms: Room[];
    private var currentRoom: string;
    private var isRunning: bool;
    
    function Game() {
        this.rooms = [];
        this.isRunning = true;
        SetupRooms();
    }
    
    private function SetupRooms(): void {
        // 创建房间
        var entrance: Room = new Room("Entrance", "You are at the entrance of a dark cave. There is a faint light coming from the north.");
        var hallway: Room = new Room("Hallway", "You are in a long hallway. You can see paintings on the walls.");
        var treasureRoom: Room = new Room("Treasure Room", "You've found the treasure room! Gold and jewels are scattered everywhere.");
        var library: Room = new Room("Library", "You are in an ancient library filled with dusty books.");
        var dungeon: Room = new Room("Dungeon", "You are in a dark dungeon. You hear strange noises from the cells.");
        
        // 设置出口
        entrance.AddExit("north", "Hallway");
        
        hallway.AddExit("south", "Entrance");
        hallway.AddExit("north", "Treasure Room");
        hallway.AddExit("east", "Library");
        hallway.AddExit("west", "Dungeon");
        
        treasureRoom.AddExit("south", "Hallway");
        
        library.AddExit("west", "Hallway");
        
        dungeon.AddExit("east", "Hallway");
        
        // 添加房间到游戏
        this.rooms[this.rooms.length] = entrance;
        this.rooms[this.rooms.length] = hallway;
        this.rooms[this.rooms.length] = treasureRoom;
        this.rooms[this.rooms.length] = library;
        this.rooms[this.rooms.length] = dungeon;
        
        // 设置起始房间
        this.currentRoom = "Entrance";
    }
    
    private function FindRoom(name: string): Room {
        foreach (var room in this.rooms) {
            if (room.GetName() == name) {
                return room;
            }
        }
        return null;
    }
    
    private function PrintCurrentRoom(): void {
        var room: Room = FindRoom(this.currentRoom);
        if (room != null) {
            print("\n" + room.GetName());
            print(room.GetDescription());
            
            print("\nExits:");
            var exits: string[] = room.GetExitDirections();
            for (var i: int32 = 0; i < exits.length; i = i + 1) {
                print("  " + exits[i]);
            }
        }
    }
    
    private function ProcessCommand(command: string): void {
        var parts: string[] = command.split(" ");
        var action: string = parts[0];
        
        if (action == "go" && parts.length > 1) {
            var direction: string = parts[1];
            var current: Room = FindRoom(this.currentRoom);
            if (current != null) {
                var nextRoomName: string = current.GetExit(direction);
                if (nextRoomName != "") {
                    this.currentRoom = nextRoomName;
                    PrintCurrentRoom();
                } else {
                    print("You can't go " + direction + " from here.");
                }
            }
        } else if (action == "look") {
            PrintCurrentRoom();
        } else if (action == "quit" || action == "exit") {
            this.isRunning = false;
            print("Thanks for playing!");
        } else if (action == "help") {
            PrintHelp();
        } else {
            print("I don't understand that command. Type 'help' for available commands.");
        }
    }
    
    private function PrintHelp(): void {
        print("\nAvailable commands:");
        print("  go [direction] - Move in the specified direction (north, south, east, west)");
        print("  look - Look around the current room");
        print("  help - Show this help message");
        print("  quit/exit - Exit the game");
    }
    
    public function Start(): void {
        print("Welcome to the Text Adventure Game!");
        print("Type 'help' for available commands.");
        
        PrintCurrentRoom();
        
        while (this.isRunning) {
            print("\nWhat do you want to do?");
            var input: string = read_line();
            ProcessCommand(input);
        }
    }
}

function main(): void {
    var game: Game = new Game();
    game.Start();
}
```

## 常见问题解决方案

### 问题1：如何处理用户输入

```csharp
function ReadString(prompt: string): string {
    print(prompt);
    return read_line();
}

function ReadInt(prompt: string): int32 {
    while (true) {
        var input: string = ReadString(prompt);
        var value: int32 = atoi(input);
        if (value != 0 || input == "0") {
            return value;
        }
        print("Invalid input. Please enter a valid integer.");
    }
}

function ReadFloat(prompt: string): float64 {
    while (true) {
        var input: string = ReadString(prompt);
        var value: float64 = atof(input);
        if (value != 0.0 || input == "0" || input == "0.0") {
            return value;
        }
        print("Invalid input. Please enter a valid number.");
    }
}

function main(): void {
    var name: string = ReadString("Enter your name: ");
    var age: int32 = ReadInt("Enter your age: ");
    var height: float64 = ReadFloat("Enter your height (meters): ");
    
    print("Hello, " + name + "! You are " + age + " years old and " + height + " meters tall.");
}
```

### 问题2：如何实现简单的数据结构

```csharp
// 简单的栈实现
class Stack<T> {
    private var items: T[];
    private var top: int32;
    
    function Stack() {
        this.items = [];
        this.top = -1;
    }
    
    public function Push(item: T): void {
        this.top = this.top + 1;
        this.items[this.top] = item;
    }
    
    public function Pop(): T {
        if (this.IsEmpty()) {
            throw new Exception("Stack is empty");
        }
        
        var item: T = this.items[this.top];
        this.top = this.top - 1;
        return item;
    }
    
    public function Peek(): T {
        if (this.IsEmpty()) {
            throw new Exception("Stack is empty");
        }
        
        return this.items[this.top];
    }
    
    public function IsEmpty(): bool {
        return this.top == -1;
    }
    
    public function GetSize(): int32 {
        return this.top + 1;
    }
}

// 简单的队列实现
class Queue<T> {
    private var items: T[];
    private var front: int32;
    private var rear: int32;
    
    function Queue() {
        this.items = [];
        this.front = 0;
        this.rear = -1;
    }
    
    public function Enqueue(item: T): void {
        this.rear = this.rear + 1;
        this.items[this.rear] = item;
    }
    
    public function Dequeue(): T {
        if (this.IsEmpty()) {
            throw new Exception("Queue is empty");
        }
        
        var item: T = this.items[this.front];
        this.front = this.front + 1;
        return item;
    }
    
    public function Front(): T {
        if (this.IsEmpty()) {
            throw new Exception("Queue is empty");
        }
        
        return this.items[this.front];
    }
    
    public function IsEmpty(): bool {
        return this.front > this.rear;
    }
    
    public function GetSize(): int32 {
        if (this.IsEmpty()) {
            return 0;
        }
        return this.rear - this.front + 1;
    }
}

function main(): void {
    // 测试栈
    var stack: Stack<int32> = new Stack<int32>();
    stack.Push(1);
    stack.Push(2);
    stack.Push(3);
    
    print("Stack size: " + stack.GetSize());
    print("Top item: " + stack.Peek());
    
    while (!stack.IsEmpty()) {
        print("Popped: " + stack.Pop());
    }
    
    // 测试队列
    var queue: Queue<string> = new Queue<string>();
    queue.Enqueue("First");
    queue.Enqueue("Second");
    queue.Enqueue("Third");
    
    print("Queue size: " + queue.GetSize());
    print("Front item: " + queue.Front());
    
    while (!queue.IsEmpty()) {
        print("Dequeued: " + queue.Dequeue());
    }
}
```

### 问题3：如何实现简单的排序算法

```csharp
// 冒泡排序
function BubbleSort(arr: int32[]): void {
    var n: int32 = arr.length;
    for (var i: int32 = 0; i < n - 1; i = i + 1) {
        for (var j: int32 = 0; j < n - i - 1; j = j + 1) {
            if (arr[j] > arr[j + 1]) {
                // 交换元素
                var temp: int32 = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

// 选择排序
function SelectionSort(arr: int32[]): void {
    var n: int32 = arr.length;
    for (var i: int32 = 0; i < n - 1; i = i + 1) {
        var minIndex: int32 = i;
        for (var j: int32 = i + 1; j < n; j = j + 1) {
            if (arr[j] < arr[minIndex]) {
                minIndex = j;
            }
        }
        
        // 交换元素
        var temp: int32 = arr[i];
        arr[i] = arr[minIndex];
        arr[minIndex] = temp;
    }
}

// 插入排序
function InsertionSort(arr: int32[]): void {
    var n: int32 = arr.length;
    for (var i: int32 = 1; i < n; i = i + 1) {
        var key: int32 = arr[i];
        var j: int32 = i - 1;
        
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j = j - 1;
        }
        
        arr[j + 1] = key;
    }
}

// 打印数组
function PrintArray(arr: int32[]): void {
    print("[");
    for (var i: int32 = 0; i < arr.length; i = i + 1) {
        print(arr[i]);
        if (i < arr.length - 1) {
            print(", ");
        }
    }
    print("]");
}

function main(): void {
    var numbers: int32[] = [64, 34, 25, 12, 22, 11, 90];
    
    print("Original array:");
    PrintArray(numbers);
    
    // 测试冒泡排序
    var bubbleArray: int32[] = [64, 34, 25, 12, 22, 11, 90];
    BubbleSort(bubbleArray);
    print("\nAfter Bubble Sort:");
    PrintArray(bubbleArray);
    
    // 测试选择排序
    var selectionArray: int32[] = [64, 34, 25, 12, 22, 11, 90];
    SelectionSort(selectionArray);
    print("\nAfter Selection Sort:");
    PrintArray(selectionArray);
    
    // 测试插入排序
    var insertionArray: int32[] = [64, 34, 25, 12, 22, 11, 90];
    InsertionSort(insertionArray);
    print("\nAfter Insertion Sort:");
    PrintArray(insertionArray);
}
```

---

*这些示例和教程涵盖了E#语言的各种特性和应用场景，帮助开发者快速上手并掌握E#编程。*