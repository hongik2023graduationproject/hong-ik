# hong-ik 기여 가이드 (Contributing Guide)

이 문서는 프로젝트에 기여하는 방법을 안내합니다.

---

## 개발 환경 설정 (Setup)

### 필요한 도구

- **C++26**을 지원하는 최신 컴파일러 (네이티브 빌드는 `CMAKE_CXX_STANDARD 26`, WASM 빌드는 Emscripten으로 C++20)
- **CMake** 3.28 이상
- **Git**

### 저장소 클론

```bash
git clone https://github.com/<your-fork>/hong-ik.git
cd hong-ik
```

---

## 빌드 및 테스트 (Build & Test)

### 빌드

빌드와 실행 명령은 [README의 시작하기](README.md#시작하기)에 있습니다. WASM 빌드는 `scripts/build-wasm.sh`(Emscripten 필요)로 합니다.

### 테스트 실행

프로젝트는 Google Test 기반의 단위 테스트를 사용합니다.

```bash
# 전체 테스트 실행
ctest --test-dir build --output-on-failure

# 특정 테스트만 실행 (예: 렉서 테스트)
build/HongIkTests --gtest_filter="LexerTest.*"
```

PR을 제출하기 전에 모든 테스트가 통과하는지 확인합니다. CI는 Windows, Linux, macOS 세 플랫폼에서 빌드와 테스트를 수행합니다.

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

대부분의 IDE(CLion, VS Code 등)는 `.clang-format` 파일을 인식해 저장 시 포맷을 적용할 수 있습니다.

---

## 커밋 메시지 규칙 (Commit Messages)

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

- 제목 50자 이내
- 한국어·영어 모두 가능

---

## Pull Request 규칙 (PR Guidelines)

### PR 제출 전 체크리스트

- [ ] 모든 테스트 통과 (`ctest --test-dir build`)
- [ ] 새로운 기능의 테스트 작성
- [ ] `.clang-format`에 맞춘 코드 포맷
- [ ] 규칙에 맞는 커밋 메시지

### PR 작성 요령

1. **제목**: 변경 내용 한 줄 요약
2. **본문**: 무엇을 왜 변경했는지 설명
3. **관련 이슈**: 관련 이슈가 있으면 `Closes #이슈번호` 포함

### 브랜치 전략

- `main` 브랜치 직접 푸시 금지
- 기능 브랜치에서 작업한 뒤 PR 제출
- 브랜치 이름 예시: `feat/foreach-loop`, `fix/negative-index-crash`

---

## 이슈 등록 (Issues)

버그 제보, 기능 제안, 질문은 GitHub Issues로 받습니다. 이슈 템플릿 양식에 맞춰 작성합니다.

- **버그 리포트**: 재현 단계와 환경 정보 포함
- **기능 요청**: 어떤 기능이 왜 필요한지 설명

---

## 프로젝트 구조 (Project Structure)

디렉터리 구성과 실행 파이프라인은 [ARCHITECTURE.md](ARCHITECTURE.md)에 있습니다.
