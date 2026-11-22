# CLAUDE.md

Claude Code가 이 저장소에서 작업할 때 보는 지침입니다. 실행 파이프라인과 디렉터리, 지켜야 할 규칙은 `ARCHITECTURE.md`, 빌드·테스트·코드 스타일은 `CONTRIBUTING.md`, 언어 문법과 내장 함수 목록은 `README.md`에 있습니다.

## 검증

- 끝내기 전에 `cmake --build build`와 `ctest --test-dir build --output-on-failure --timeout 120`을 돌립니다. ctest에는 항상 `--timeout`을 줍니다.
- 한 묶음만 볼 때는 `build/HongIkTests --gtest_filter="LexerTest.*"`처럼 거릅니다.
- 고친 C++ 파일은 `clang-format -i`로 맞춥니다.

## 언어 기능을 고칠 때

- 기본 백엔드는 VM(`vm/compiler*.cpp`, VM)이고 `--eval`이 Evaluator입니다. 고차 함수(`매핑`·`걸러내기`·`줄이기`)와 `부모.메서드()`는 지금 `--eval`에서만 동작합니다. 기능을 고치면 두 백엔드에서 각각 동작을 확인합니다.
- 레지스트리의 `반복`(문자열 반복)은 반복문 키워드와 겹쳐 호출할 수 없습니다. 내장 함수 이름을 정할 때 키워드와 겹치지 않는지 봅니다.

## Sanitizer

CI(`.github/workflows/sanitizers.yml`)는 Linux clang에서 ASan+UBSan과 TSan을 따로 돌립니다. 로컬 Windows에서는 아래 범위까지 재현합니다.

```bash
# MSVC ASan (MSVC는 UBSan·TSan·LeakSanitizer 미지원)
cmake -B build-asan -G "Visual Studio 17 2022" -A x64 -DHONGIK_ENABLE_SANITIZERS=ON -DHONGIK_SANITIZERS=address
cmake --build build-asan --config RelWithDebInfo
# 실행 전 ASan 런타임 DLL 폴더를 PATH에 넣습니다. 없으면 테스트마다 0xc0000135 오류 창을 띄우고 멈춥니다.
export PATH="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/<버전>/bin/Hostx64/x64:$PATH"
ctest --test-dir build-asan --build-config RelWithDebInfo --output-on-failure --timeout 120

# MinGW g++ UBSan: 런타임이 없어 trap 모드만 가능(메시지 없이 0xC000001D로 종료)
cmake -B build-ubsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fsanitize-trap=undefined"
```

- Git Bash에서 `/O2`처럼 `/`로 시작하는 MSVC 옵션을 넘기면 MSYS가 경로로 바꿉니다. `-O2` 형식을 씁니다.
