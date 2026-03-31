# hong-ik: A Korean Programming Language

> Write code in Hangul. Think in Korean. Program beautifully.

## Introduction

**hong-ik** is a programming language designed entirely around the Korean alphabet (Hangul). Every keyword, every built-in function, every type declaration -- all in Korean. Written in C++, it ships with both a tree-walking interpreter and a bytecode VM.

If you've ever wondered what it would look like to write `if` as `만약` (pronounced *man-yak*, meaning "if") or declare a function with `함수` (*ham-su*, meaning "function"), this is the project for you.

```
// "Hello, World!" in hong-ik
출력("안녕하세요, 세계!")
```

## Installation

### Requirements
- C++17 or later
- [CMake](https://cmake.org/) 3.x or later

### Build
```bash
cmake -B cmake-build-debug
cmake --build cmake-build-debug
```

### Test
```bash
ctest --test-dir cmake-build-debug
```

### Run
```bash
# Execute a file
./hong-ik filename.hik

# Start REPL mode
./hong-ik

# Run with bytecode VM
./hong-ik --vm filename.hik
```

## Language Syntax

### Variable Declaration

Korean type keywords: `정수` (integer), `실수` (float), `문자` (string), `논리` (boolean), `배열` (array), `사전` (dictionary).

```
정수 x = 10           // int x = 10
실수 pi = 3.14        // float pi = 3.14
문자 이름 = "홍길동"    // string name = "Hong Gil-dong"
논리 참 = true         // bool flag = true
배열 목록 = [1, 2, 3]  // array list = [1, 2, 3]
사전 정보 = {"이름": "홍길동", "나이": "25"}  // dict info = {...}
```

### Null Type

`없음` means "nothing" -- hong-ik's equivalent of `null`.

```
정수 x = 없음       // int x = null
x == 없음           // true
```

### String Interpolation

```
문자 이름 = "홍길동"
문자 인사 = "안녕 {이름}님"    // "안녕 홍길동님" ("Hello, Hong Gil-dong")
```

### Conditionals

`만약` (if), `라면` (then), `아니면` (else).

```
만약 x > 5 라면:       // if x > 5:
    출력("크다")        //     print("big")
아니면:                 // else:
    출력("작다")        //     print("small")
```

### While Loop

`반복` (repeat), `동안` (while/during).

```
반복 i < 10 동안:      // while i < 10:
    i += 1
```

### Range For Loop

`부터` (from), `까지` (to/until).

```
반복 정수 i = 0 부터 10 까지:    // for int i = 0 to 10:
    출력(i)                       //     print(i)
```

### ForEach Loop

`각각` (each), `에서` (from/in).

```
각각 정수 원소 [1, 2, 3] 에서:   // for each int elem in [1, 2, 3]:
    출력(원소)                     //     print(elem)
```

### Functions

`함수` (function), `리턴` (return).

```
함수 더하기(정수 a, 정수 b) -> 정수:   // function add(int a, int b) -> int:
    리턴 a + b                          //     return a + b

더하기(3, 7)    // 10
```

### Default Parameters

```
함수 인사(문자 이름, 문자 접미사 = "님") -> 문자:   // function greet(string name, string suffix = "님")
    리턴 이름 + 접미사

인사("홍길동")        // "홍길동님"
인사("홍길동", "씨")  // "홍길동씨"
```

### Higher-Order Functions (Lambda/Map/Filter/Reduce)

`매핑` (map), `걸러내기` (filter), `줄이기` (reduce).

```
함수 두배(정수 x) -> 정수:       // function double(int x) -> int:
    리턴 x * 2                    //     return x * 2

매핑([1, 2, 3], 두배)            // map([1,2,3], double)     -> [2, 4, 6]
걸러내기([1, -2, 3], 양수)       // filter([1,-2,3], positive) -> [1, 3]
줄이기([1, 2, 3], 합, 0)         // reduce([1,2,3], sum, 0)  -> 6
```

### Match/Switch

`비교` (compare/match), `경우` (case), `기본` (default).

```
비교 x:                // match x:
    경우 1:             //     case 1:
        출력("일")      //         print("one")
    경우 2:             //     case 2:
        출력("이")      //         print("two")
    기본:               //     default:
        출력("기타")    //         print("other")
```

### Tuples

```
(1, 2, 3)[0]    // 1
```

### Classes / OOP

`클래스` (class), `생성` (constructor), `자기` (self/this).

```
클래스 동물:                              // class Animal:
    문자 이름                              //     string name
    정수 나이                              //     int age
    생성(문자 이름, 정수 나이):             //     constructor(string name, int age):
        자기.이름 = 이름                    //         this.name = name
        자기.나이 = 나이                    //         this.age = age
    함수 소개() -> 문자:                   //     function introduce() -> string:
        리턴 자기.이름                      //         return this.name

동물 강아지 = 동물("뽀삐", 3)             // Animal puppy = Animal("Poppy", 3)
강아지.이름      // "뽀삐" ("Poppy")
강아지.소개()    // "뽀삐"
```

### Inheritance

```
클래스 강아지 < 동물:                  // class Dog < Animal:
    함수 소리() -> 문자:               //     function sound() -> string:
        리턴 "멍멍"                     //         return "woof"

강아지 뽀삐 = 강아지("뽀삐", 3)       // Dog poppy = Dog("Poppy", 3)
뽀삐.소개()    // parent method works  -> "뽀삐"
뽀삐.소리()    // "멍멍" ("woof")
```

### Exception Handling

`시도` (try), `실패` (catch/fail).

```
시도:                   // try:
    정수 x = 10 / 0     //     int x = 10 / 0
실패 오류:              // catch error:
    출력(오류)           //     print(error)
```

### Module Import

`가져오기` (import).

```
가져오기 "파일명.hik"    // import "filename.hik"
```

### Negative Indexing

```
배열 목록 = [10, 20, 30]
목록[-1]    // 30
목록[-2]    // 20
```

### Increment/Decrement Operators

```
정수 x = 5
x++    // 6
x--    // 5
```

### Exponentiation Operator

```
2 ** 10    // 1024
```

## Built-in Functions (46)

### I/O (4)

| Function | Description | Example |
|----------|-------------|---------|
| `출력(value)` | Print to console | `출력("안녕")` |
| `입력(prompt)` | Read user input | `입력("이름: ")` |
| `파일읽기(path)` | Read a file | `파일읽기("data.txt")` |
| `파일쓰기(path, content)` | Write to a file | `파일쓰기("out.txt", "내용")` |

### Type & Conversion (4)

| Function | Description | Example |
|----------|-------------|---------|
| `타입(value)` | Get type as string | `타입(42)` -> `"정수"` |
| `정수변환(value)` | Convert to integer | `정수변환("42")` -> `42` |
| `실수변환(value)` | Convert to float | `실수변환("3.14")` -> `3.14` |
| `문자변환(value)` | Convert to string | `문자변환(42)` -> `"42"` |

### Collection (7)

| Function | Description | Example |
|----------|-------------|---------|
| `길이(value)` | Length of string/array/dict | `길이("abc")` -> `3` |
| `추가(array, value)` | Push to array | `추가(목록, 4)` |
| `키목록(dict)` | Get keys as array | `키목록(정보)` |
| `포함(collection, value)` | Check membership | `포함("hello", "ell")` -> `true` |
| `설정(dict, key, value)` | Set dict entry | `설정(정보, "키", "값")` |
| `삭제(collection, key)` | Remove element | `삭제(목록, 0)` |
| `찾기(array, value)` | Find index | `찾기([10,20], 20)` -> `1` |

### Array Operations (4)

| Function | Description | Example |
|----------|-------------|---------|
| `정렬(array)` | Sort array | `정렬([3,1,2])` -> `[1,2,3]` |
| `뒤집기(array/string)` | Reverse | `뒤집기([1,2,3])` -> `[3,2,1]` |
| `조각(array, start, end)` | Slice subarray | `조각([1,2,3,4], 1, 3)` -> `[2,3]` |

### Higher-Order Functions (3)

| Function | Description | Example |
|----------|-------------|---------|
| `매핑(array, fn)` | Map | `매핑([1,2,3], 두배)` |
| `걸러내기(array, fn)` | Filter | `걸러내기([1,-2,3], 양수)` |
| `줄이기(array, fn, init)` | Reduce | `줄이기([1,2,3], 합, 0)` |

### String Operations (10)

| Function | Description | Example |
|----------|-------------|---------|
| `분리(string, delimiter)` | Split string | `분리("a,b", ",")` -> `["a", "b"]` |
| `대문자(string)` | To uppercase | `대문자("hello")` -> `"HELLO"` |
| `소문자(string)` | To lowercase | `소문자("HELLO")` -> `"hello"` |
| `치환(original, target, replacement)` | Replace | `치환("hello", "l", "r")` -> `"herro"` |
| `자르기(string)` | Trim whitespace | `자르기("  hi  ")` -> `"hi"` |
| `시작확인(string, prefix)` | Starts with | `시작확인("hello", "he")` -> `true` |
| `끝확인(string, suffix)` | Ends with | `끝확인("hello", "lo")` -> `true` |
| `반복(string, count)` | Repeat string | `반복("ha", 3)` -> `"hahaha"` |
| `채우기(string, length)` | Pad string | `채우기("hi", 5)` -> `"hi   "` |
| `부분문자(string, start, end)` | Substring | `부분문자("hello", 1, 3)` -> `"el"` |

### Math (16)

| Function | Description | Example |
|----------|-------------|---------|
| `절대값(number)` | Absolute value | `절대값(-42)` -> `42` |
| `제곱근(number)` | Square root | `제곱근(16)` -> `4.0` |
| `최대(a, b)` | Maximum | `최대(10, 20)` -> `20` |
| `최소(a, b)` | Minimum | `최소(10, 20)` -> `10` |
| `난수(min, max)` | Random number | `난수(1, 100)` |
| `사인(number)` | Sine | `사인(0)` -> `0.0` |
| `코사인(number)` | Cosine | `코사인(0)` -> `1.0` |
| `탄젠트(number)` | Tangent | `탄젠트(0)` -> `0.0` |
| `로그(number)` | Log base 10 | `로그(100)` -> `2.0` |
| `자연로그(number)` | Natural log (ln) | `자연로그(1)` -> `0.0` |
| `거듭제곱(base, exp)` | Power | `거듭제곱(2, 10)` -> `1024` |
| `파이()` | Pi constant | `파이()` -> `3.14159...` |
| `자연수e()` | Euler's number e | `자연수e()` -> `2.71828...` |
| `반올림(number)` | Round | `반올림(3.6)` -> `4` |
| `올림(number)` | Ceiling | `올림(3.1)` -> `4` |
| `내림(number)` | Floor | `내림(3.9)` -> `3` |

### JSON (2)

| Function | Description | Example |
|----------|-------------|---------|
| `JSON_파싱(string)` | Parse JSON string | `JSON_파싱("{\"a\": 1}")` |
| `JSON_직렬화(value)` | Serialize to JSON | `JSON_직렬화(사전)` |

## Bytecode VM

Use the `--vm` flag to run programs on the bytecode virtual machine instead of the tree-walking interpreter.

```bash
./hong-ik --vm filename.hik
```

The VM compiles source code into bytecode instructions, then executes them on a stack-based virtual machine. It supports most language features including variables, functions, classes, exception handling, closures, and constant folding optimizations.

## Architecture

```
Source (.hik)  ->  Lexer  ->  Parser  ->  Evaluator  ->  Result
                    |           |            |
                  Tokens       AST      Environment

               or (VM mode)

Source (.hik)  ->  Lexer  ->  Parser  ->  Compiler  ->  VM  ->  Result
                    |           |            |           |
                  Tokens       AST       Bytecode     Stack
```

| Directory | Role |
|-----------|------|
| `lexer/` | Tokenizes Korean source code |
| `token/` | Token type definitions |
| `parser/` | Tokens to AST |
| `ast/` | AST node definitions (statements, expressions, literals) |
| `evaluator/` | Tree-walking execution engine |
| `environment/` | Variable scopes and bindings |
| `object/` | Runtime object types and built-in functions |
| `compiler/` | Bytecode compiler |
| `vm/` | Bytecode virtual machine |
| `utf8_converter/` | Hangul (UTF-8) encoding utilities |
| `repl/` | REPL and file execution entry point |
| `exception/` | Exception definitions |
| `tests/` | Google Test unit tests |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup instructions, code style guidelines, commit message conventions, and PR rules. Contributions in both Korean and English are welcome!

## License

This project is part of the Hongik University project initiative.
