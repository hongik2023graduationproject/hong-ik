# 개발·검증 가이드

## 빌드
1. 서버: 저장소 루트에서 `cmake --build cmake-build-debug --target hongik-lsp`
2. 클라이언트: `cd vscode-extension && npm install && npm run compile`

## Extension Host로 검증 (F5)
1. VSCode에서 `vscode-extension/` 폴더를 열고 F5 (Run Extension)
2. 설정에서 `hongik.lsp.path`를 `<repo>/cmake-build-debug/hongik-lsp(.exe)` 절대 경로로 지정
3. `.hik` 파일을 열고 아래 체크리스트 확인

## 체크리스트 (v1 기능 5종)
- [ ] 진단: `정수 x = ;` 입력 → 빨간 밑줄 + 한글 에러 메시지, 고치면 사라짐
- [ ] 진단(타입): `정수 x = 모름` → 타입 체커 경고/에러 표시
- [ ] 호버: 선언된 변수 위에 마우스 → `정수 x` 타입 표시, `출력` 위 → 내장 함수 시그니처
- [ ] 자동완성: Ctrl+Space → 키워드(만약/함수/클래스…), 내장 함수 45종, 문서 내 심볼
- [ ] 정의로 이동: 변수 사용처에서 F12 → 선언 줄로 점프
- [ ] 아웃라인: 탐색기 아웃라인에 함수/클래스(필드·메서드 children)/전역 변수 표시
- [ ] 한글 위치 정확성: `정수 한글이름 = 1` 에서 호버/밑줄 범위가 글자 단위로 정확한지

## 알려진 v1 한계 (spec v1.1)
- 자동완성 스코프 필터링 없음 (문서 전체 심볼 노출)
- 렉서 에러(알 수 없는 문자)는 해당 파일 진단이 1건만 표시됨
- 멀티 파일(`가져오기`) 심볼은 미지원
