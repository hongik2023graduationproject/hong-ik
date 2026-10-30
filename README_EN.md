# hong-ik: A Korean Programming Language

> Write code in Hangul. Think in Korean. Program beautifully.

## Introduction

**hong-ik** is a programming language designed entirely around the Korean alphabet (Hangul). Every keyword, every built-in function, every type declaration -- all in Korean. Written in C++, it ships with both a bytecode VM (the default) and a tree-walking interpreter (`--eval`).

If you've ever wondered what it would look like to write `if` as `만약` (pronounced *man-yak*, meaning "if") or declare a function with `함수` (*ham-su*, meaning "function"), this is the project for you.

```
// "Hello, World!" in hong-ik
출력("안녕하세요, 세계!")
```

## Installation

### Requirements
- A recent compiler with C++26 support (the native build sets `CMAKE_CXX_STANDARD 26`)
- [CMake](https://cmake.org/) 3.28 or later
- (Optional) WASM build: [Emscripten](https://emscripten.org/) -- the WASM target is compiled as C++20 (`scripts/build-wasm.sh`)

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
# Execute a file (default: bytecode VM)
./cmake-build-debug/HongIk filename.hik

# Start REPL mode
./cmake-build-debug/HongIk

# Run with the tree-walking interpreter
./cmake-build-debug/HongIk --eval filename.hik

# Static type checking mode (off|warn|strict, default warn)
./cmake-build-debug/HongIk --type-check=strict filename.hik
```

With `--type-check=warn` (the default) type errors found before execution are printed as `type-warning[...]` and the program still runs; `strict` prints them as `type-error[...]` and does not run the program; `off` skips the check.

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

The value must match the declared type -- there is no implicit promotion (`실수 r = 1` is an error; write `실수 r = 1.0`).

### Null Type and Optionals

`없음` means "nothing" -- hong-ik's equivalent of `null`. A plain typed variable cannot hold `없음`; append `?` to the type to make it Optional.

```
정수? x = 없음      // int? x = null
x == 없음           // true
x = 3               // an Optional can also hold a regular value

// 정수 y = 없음    // error: '없음' cannot be assigned to a non-optional variable
```

### String Interpolation

```
문자 이름 = "홍길동"
문자 인사 = "안녕 {이름}님"    // "안녕 홍길동님" ("Hello, Hong Gil-dong")
```

`{...}` inside a string literal is always treated as interpolation. If you need braces in a string (e.g. a JSON object), split it and concatenate: `"{" + "\"a\": 1}"`.

### Conditionals

`만약` (if), `라면` (then), `아니면` (else).

```
정수 x = 7
만약 x > 5 라면:       // if x > 5:
    출력("크다")        //     print("big")
아니면:                 // else:
    출력("작다")        //     print("small")
```

### While Loop

`반복` (repeat), `동안` (while/during).

```
정수 i = 0
반복 i < 10 동안:      // while i < 10:
    i += 1
```

### Range For Loop

`부터` (from), `까지` (to/until).

```
반복 정수 i = 0 부터 10 까지:    // for int i = 0 to 10 (0..9, end exclusive):
    출력(i)                       //     print(i)
```

### ForEach Loop

`각각` (each), `에서` (from/in).

```
각각 정수 원소 [1, 2, 3] 에서:   // for each int elem in [1, 2, 3]:
    출력(원소)                     //     print(elem)
```

### Break / Continue

`중단` (break), `계속` (continue).

```
정수 i = 0
반복 i < 10 동안:
    i += 1
    만약 i == 2 라면:
        계속          // skip 2
    만약 i == 5 라면:
        중단          // stop at 5
    출력(i)           // 1, 3, 4
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

### Generators

`생산` (yield) emits values one at a time; iterate them with `각각`.

```
함수 세기(정수 끝) -> 정수:      // function count(int end) -> int:
    정수 i = 0
    반복 i < 끝 동안:
        생산 i                    //         yield i
        i += 1

각각 정수 n 세기(3) 에서:
    출력(n)    // 0, 1, 2
```

> The static type checker currently emits a TC302 warning when iterating a generator; execution is unaffected.

### Higher-Order Functions (Lambda/Map/Filter/Reduce)

`매핑` (map), `걸러내기` (filter), `줄이기` (reduce). These currently work **only in the tree-walking interpreter (`--eval`)**; the default VM reports them as unknown identifiers.

```
함수 두배(정수 x) -> 정수:       // function double(int x) -> int:
    리턴 x * 2                    //     return x * 2

함수 양수(정수 x) -> 논리:       // function positive(int x) -> bool:
    리턴 x > 0

함수 합(정수 누적, 정수 x) -> 정수:  // function sum(int acc, int x) -> int:
    리턴 누적 + x

매핑([1, 2, 3], 두배)            // map([1,2,3], double)     -> [2, 4, 6]
걸러내기([1, -2, 3], 양수)       // filter([1,-2,3], positive) -> [1, 3]
줄이기([1, 2, 3], 합, 0)         // reduce([1,2,3], sum, 0)  -> 6
```

### Match/Switch

`비교` (compare/match), `경우` (case), `기본` (default).

```
정수 x = 2
비교 x:                // match x:
    경우 1:             //     case 1:
        출력("일")      //         print("one")
    경우 2:             //     case 2:
        출력("이")      //         print("two")
    기본:               //     default:
        출력("기타")    //         print("other")
```

`경우 start~end:` matches an inclusive range.

```
정수 점수 = 85
비교 점수:
    경우 90~100:
        출력("A")
    경우 80~89:
        출력("B")    // printed
    기본:
        출력("F")
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

Continuing from the `동물` class above:

```
클래스 강아지 < 동물:                  // class Dog < Animal:
    함수 소리() -> 문자:               //     function sound() -> string:
        리턴 "멍멍"                     //         return "woof"

강아지 뽀삐 = 강아지("뽀삐", 3)       // Dog poppy = Dog("Poppy", 3)
뽀삐.소개()    // parent method works  -> "뽀삐"
뽀삐.소리()    // "멍멍" ("woof")
```

Inside a method, `부모` (parent/super) calls the parent class's method. This currently works **only with `--eval`**; the default VM reports `부모` as an unknown identifier.

```
클래스 동물:
    문자 이름
    생성(문자 이름):
        자기.이름 = 이름
    함수 소개() -> 문자:
        리턴 "저는 " + 자기.이름

클래스 강아지 < 동물:
    함수 소개() -> 문자:
        리턴 부모.소개() + " (강아지)"   // super.introduce() + " (dog)"

강아지 뽀삐 = 강아지("뽀삐")
뽀삐.소개()    // "저는 뽀삐 (강아지)"
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
2 ** 100   // overflows int, so the result becomes a float (1267650600228229401496703205376.0)
```

Other integer arithmetic (`+`, `-`, `*`, ...) raises a runtime error ("정수 연산 결과가 범위를 넘었습니다.") when the result does not fit in a 64-bit integer.

### Logical / Bitwise Operators

```
true && false    // false
true || false    // true
!true            // false
6 & 3            // 2 (bitwise AND)
6 | 3            // 7 (bitwise OR)
```

> `~` is not bitwise NOT -- it is only used for range patterns in `비교` (`경우 1~5:`). `...` is recognized by the lexer, but variadic parameters are not implemented yet.

## Built-in Functions (45 + 3 higher-order)

There are 45 regular built-ins (registered in `object/builtin_registry.cpp`) plus 3 higher-order functions (`매핑`, `걸러내기`, `줄이기`) handled specially by the interpreter (currently `--eval` only). The registry also contains `반복` (string repeat), but it cannot be called because `반복` is the loop keyword, so it is not counted.

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

### Array Operations (3)

| Function | Description | Example |
|----------|-------------|---------|
| `정렬(array)` | Sort array | `정렬([3,1,2])` -> `[1,2,3]` |
| `뒤집기(array/string)` | Reverse | `뒤집기([1,2,3])` -> `[3,2,1]` |
| `조각(array, start, end)` | Slice subarray | `조각([1,2,3,4], 1, 3)` -> `[2,3]` |

### Higher-Order Functions (3, `--eval` only)

| Function | Description | Example |
|----------|-------------|---------|
| `매핑(array, fn)` | Map | `매핑([1,2,3], 두배)` -> `[2,4,6]` |
| `걸러내기(array, fn)` | Filter | `걸러내기([1,-2,3], 양수)` -> `[1,3]` |
| `줄이기(array, fn, init)` | Reduce | `줄이기([1,2,3], 합, 0)` -> `6` |

### String Operations (9)

| Function | Description | Example |
|----------|-------------|---------|
| `분리(string, delimiter)` | Split string | `분리("a,b", ",")` -> `["a", "b"]` |
| `대문자(string)` | To uppercase | `대문자("hello")` -> `"HELLO"` |
| `소문자(string)` | To lowercase | `소문자("HELLO")` -> `"hello"` |
| `치환(original, target, replacement)` | Replace | `치환("hello", "l", "r")` -> `"herro"` |
| `자르기(string)` | Trim whitespace | `자르기("  hi  ")` -> `"hi"` |
| `시작확인(string, prefix)` | Starts with | `시작확인("hello", "he")` -> `true` |
| `끝확인(string, suffix)` | Ends with | `끝확인("hello", "lo")` -> `true` |
| `채우기(string, length, char)` | Pad string (right) | `채우기("hi", 5, " ")` -> `"hi   "` |
| `부분문자(string, start, end)` | Substring | `부분문자("hello", 1, 3)` -> `"el"` (end exclusive) |

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
| `거듭제곱(base, exp)` | Power | `거듭제곱(2, 10)` -> `1024.0` |
| `파이()` | Pi constant | `파이()` -> `3.14159...` |
| `자연수e()` | Euler's number e | `자연수e()` -> `2.71828...` |
| `반올림(number)` | Round | `반올림(3.6)` -> `4` |
| `올림(number)` | Ceiling | `올림(3.1)` -> `4` |
| `내림(number)` | Floor | `내림(3.9)` -> `3` |

### JSON (2)

| Function | Description | Example |
|----------|-------------|---------|
| `JSON_파싱(string)` | Parse JSON string | `JSON_파싱("[1, 2]")` -> `[1, 2]` |
| `JSON_직렬화(value)` | Serialize to JSON | `JSON_직렬화({"a": 1})` -> `"{\"a\":1}"` |

## Bytecode VM

The bytecode VM is the **default backend**. Use `--eval` to run programs on the tree-walking interpreter instead. The `--vm` flag is still accepted for compatibility but is a no-op.

```bash
./cmake-build-debug/HongIk filename.hik          # VM (default)
./cmake-build-debug/HongIk --eval filename.hik   # tree-walking interpreter
```

The VM compiles source code into bytecode instructions, then executes them on a stack-based virtual machine. It supports most language features including variables, functions, classes, exception handling, closures, and constant folding optimizations. Higher-order functions (`매핑`/`걸러내기`/`줄이기`) and `부모` calls currently work only with `--eval`.

## Editor Support (LSP)

`hongik-lsp` is a Language Server that speaks JSON-RPC over stdio. It provides diagnostics (syntax/type errors), hover, completion, go-to-definition, and document symbols (outline).

### Build
```bash
cmake --build cmake-build-debug --target hongik-lsp
```

### VSCode Extension
```bash
cd vscode-extension
npm install
npm run compile
```

Set `hongik.lsp.path` in VSCode to the absolute path of the built `hongik-lsp(.exe)` and the extension will launch it; if empty, it looks up `hongik-lsp` on PATH. See [`vscode-extension/DEVELOPMENT.md`](./vscode-extension/DEVELOPMENT.md) for the verification steps.

## Architecture

```
Source (.hik)  ->  Lexer  ->  Parser  ->  Compiler  ->  VM  ->  Result   (default)
                    |           |            |           |
                  Tokens       AST       Bytecode     Stack

               or (--eval)

Source (.hik)  ->  Lexer  ->  Parser  ->  Evaluator  ->  Result
                    |           |            |
                  Tokens       AST      Environment
```

| Directory | Role |
|-----------|------|
| `lexer/` | Tokenizes Korean source code |
| `token/` | Token type definitions |
| `parser/` | Tokens to AST |
| `ast/` | AST node definitions (statements, expressions, literals) |
| `analyzer/` | Syntax highlighting, symbol collection, static type checker |
| `vm/` | Bytecode compiler (`compiler*.cpp`), peephole optimizer, and VM -- the default backend |
| `evaluator/` | Tree-walking execution engine (`--eval`) |
| `environment/` | Variable scopes and bindings |
| `object/` | Runtime object types and built-in functions |
| `error/` | Structured error type (`HongIkError`, with source location) |
| `exception/` | Exception definitions |
| `io/` | Callback-based I/O interface (`IOContext`: console / web) |
| `sandbox/` | Execution time and loop-iteration limits (`ExecutionLimiter`) |
| `util/` | Shared helpers (overflow-checked integer ops, static type model, JSON, UTF-8) |
| `utf8_converter/` | Hangul (UTF-8) encoding utilities |
| `repl/` | REPL and file execution entry point |
| `lsp/` | Language Server (`hongik-lsp`) |
| `web/` | WASM (Emscripten) bindings and in-memory filesystem |
| `vscode-extension/` | VSCode extension (syntax highlighting, snippets, LSP client) |
| `benchmarks/` | Google Benchmark suite (`HongIkBenchmarks`) |
| `scripts/` | Build scripts (`build-wasm.sh`) |
| `docs/` | Design specs and benchmark results |
| `tests/` | Google Test unit tests |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup instructions, code style guidelines, commit message conventions, and PR rules. Contributions in both Korean and English are welcome!

## License

This project is part of the Hongik University project initiative.
