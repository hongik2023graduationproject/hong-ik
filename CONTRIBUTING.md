# hong-ik 기여 가이드 (Contributing Guide)

hong-ik 프로젝트에 관심을 가져주셔서 감사합니다! 한글 프로그래밍 언어를 함께 만들어가는 여정에 여러분을 환영합니다.

이 문서는 프로젝트에 기여하는 방법을 안내합니다. 처음 오픈소스에 기여하시는 분도 편하게 따라오실 수 있도록 작성했습니다.

---

## 목차

1. [개발 환경 설정 (Setup)](#개발-환경-설정-setup)
2. [빌드 및 테스트 (Build & Test)](#빌드-및-테스트-build--test)
3. [코드 스타일 (Code Style)](#코드-스타일-code-style)
4. [커밋 메시지 규칙 (Commit Messages)](#커밋-메시지-규칙-commit-messages)
5. [Pull Request 규칙 (PR Guidelines)](#pull-request-규칙-pr-guidelines)
6. [이슈 등록 (Issues)](#이슈-등록-issues)
7. [프로젝트 구조 (Project Structure)](#프로젝트-구조-project-structure)

---

## 개발 환경 설정 (Setup)

### 필요한 도구

- **C++17 이상** 지원 컴파일러 (GCC 9+, Clang 10+, MSVC 2019+)
- **CMake** 3.x 이상
- **Git**

### 저장소 클론

```bash
git clone https://github.com/<your-fork>/hong-ik.git
cd hong-ik
```

### 빌드 확인

클론 후 아래 명령어로 빌드가 정상적으로 되는지 확인해주세요:

```bash
cmake -B build
cmake --build build
```

---

## 빌드 및 테스트 (Build & Test)

### 빌드

```bash
# 빌드 디렉토리 생성 및 설정
cmake -B build

# 빌드 실행
cmake --build build
```

### 테스트 실행

프로젝트는 Google Test 기반의 단위 테스트를 사용합니다.

```bash
# 전체 테스트 실행
ctest --test-dir build --output-on-failure

# 특정 테스트만 실행 (예: 렉서 테스트)
build/HongIkTests --gtest_filter="LexerTest.*"
```

**PR을 제출하기 전에 반드시 모든 테스트가 통과하는지 확인해주세요.** CI에서 Windows, Linux, macOS 세 플랫폼 모두 빌드 및 테스트를 수행합니다.

### 인터프리터 실행

```bash
# 파일 실행
./build/HongIk filename.hik

# REPL 모드
./build/HongIk

# 바이트코드 VM 모드
./build/HongIk --vm filename.hik
```

---

## 코드 스타일 (Code Style)

프로젝트 루트의 `.clang-format` 파일을 기준으로 코드 스타일을 유지합니다.

### 주요 규칙

- **기반 스타일**: LLVM
- **들여쓰기**: 4칸 (스페이스)
- **줄 길이 제한**: 120자
- **포인터 정렬**: 왼쪽 (`int* ptr`)

### 포맷 적용 방법

```bash
# 단일 파일 포맷
clang-format -i path/to/file.cpp

# 변경된 파일만 포맷 (Git 기준)
git diff --name-only | xargs clang-format -i
```

대부분의 IDE(CLion, VS Code 등)에서 `.clang-format` 파일을 자동으로 인식하여 저장 시 포맷을 적용할 수 있습니다. 가능하면 이 기능을 활성화해주세요.

---

## 커밋 메시지 규칙 (Commit Messages)

명확하고 일관된 커밋 히스토리를 위해 아래 규칙을 따라주세요.

### 형식

```
<타입>: <설명>

[본문 (선택)]
```

### 타입

| 타입 | 설명 |
|------|------|
| `feat` | 새로운 기능 추가 |
| `fix` | 버그 수정 |
| `refactor` | 리팩토링 (기능 변경 없음) |
| `test` | 테스트 추가/수정 |
| `docs` | 문서 변경 |
| `chore` | 빌드, CI 등 기타 작업 |

### 예시

```
feat: ForEach 반복문 구현
fix: 음수 인덱스 범위 초과 시 크래시 수정
test: 문자열 보간 테스트 케이스 추가
docs: README에 바이트코드 VM 설명 추가
```

- 제목은 **50자 이내**로 간결하게 작성해주세요.
- 한국어 또는 영어 모두 괜찮습니다.

---

## Pull Request 규칙 (PR Guidelines)

### PR 제출 전 체크리스트

- [ ] 모든 테스트가 통과하는지 확인했습니다 (`ctest --test-dir build`)
- [ ] 새로운 기능에 대한 테스트를 작성했습니다
- [ ] `.clang-format`에 맞게 코드를 포맷했습니다
- [ ] 커밋 메시지가 규칙에 맞습니다

### PR 작성 요령

1. **제목**: 변경 내용을 한 줄로 요약해주세요.
2. **본문**: 무엇을 왜 변경했는지 설명해주세요.
3. **관련 이슈**: 관련 이슈가 있다면 `Closes #이슈번호`를 포함해주세요.

### 브랜치 전략

- `main` 브랜치에 직접 푸시하지 마세요.
- 기능 브랜치를 만들어 작업 후 PR을 제출해주세요.
- 브랜치 이름 예시: `feat/foreach-loop`, `fix/negative-index-crash`

---

## 이슈 등록 (Issues)

버그를 발견하셨거나 새로운 기능을 제안하고 싶으시다면, GitHub Issues를 통해 알려주세요. 이슈 템플릿이 준비되어 있으니 양식에 맞춰 작성해주시면 됩니다.

- **버그 리포트**: 재현 단계와 환경 정보를 포함해주세요.
- **기능 요청**: 어떤 기능이 왜 필요한지 설명해주세요.

---

## 프로젝트 구조 (Project Structure)

```
hong-ik/
├── lexer/          # 소스코드 → 토큰 변환
├── token/          # 토큰 타입 정의
├── parser/         # 토큰 → AST 변환
├── ast/            # AST 노드 정의
├── evaluator/      # AST 순회 및 실행
├── environment/    # 변수/스코프 관리
├── object/         # 런타임 객체 및 내장 함수
├── utf8_converter/ # UTF-8 인코딩 처리
├── repl/           # REPL 및 파일 실행
├── exception/      # 예외 정의
├── compiler/       # 바이트코드 컴파일러
├── vm/             # 바이트코드 가상 머신
└── tests/          # Google Test 단위 테스트
```

인터프리터 파이프라인은 `소스코드 → Lexer → Parser → Evaluator → 결과` 순서로 진행됩니다. VM 모드에서는 `소스코드 → Lexer → Parser → Compiler → VM → 결과`입니다.

---

## 도움이 필요하신가요?

기여 과정에서 막히는 부분이 있다면 언제든 이슈를 통해 질문해주세요. 한글 프로그래밍 언어를 함께 만들어가는 모든 기여를 소중히 여기고 있습니다. 감사합니다!
