#!/usr/bin/env bash
set -euo pipefail

# ============================================
# hong-ik WASM 빌드 스크립트
# ============================================
#
# 사용법:
#   ./scripts/build-wasm.sh              # 기본 빌드
#   ./scripts/build-wasm.sh --release    # 릴리스 빌드 (closure compiler 포함)
#   ./scripts/build-wasm.sh --clean      # 클린 빌드
#
# 요구사항:
#   - Emscripten SDK (emsdk) 설치 및 PATH 설정
#   - CMake 3.28+

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/cmake-build-wasm"
OUTPUT_DIR="$PROJECT_ROOT/dist/wasm"

# 기본 옵션
RELEASE_MODE=false
CLEAN_BUILD=false

# 인자 파싱
for arg in "$@"; do
    case $arg in
        --release)
            RELEASE_MODE=true
            ;;
        --clean)
            CLEAN_BUILD=true
            ;;
        --help|-h)
            echo "사용법: $0 [--release] [--clean]"
            echo "  --release    릴리스 빌드 (closure compiler, 추가 최적화)"
            echo "  --clean      빌드 디렉토리 삭제 후 클린 빌드"
            exit 0
            ;;
    esac
done

# ---- Emscripten 환경 확인 ----
if ! command -v emcmake &> /dev/null; then
    echo "[오류] emcmake를 찾을 수 없습니다."
    echo ""
    echo "Emscripten SDK를 설치하고 환경을 활성화하세요:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo "=========================================="
echo " hong-ik WASM 빌드"
echo "=========================================="
echo "프로젝트: $PROJECT_ROOT"
echo "빌드 디렉토리: $BUILD_DIR"
echo "출력 디렉토리: $OUTPUT_DIR"
echo "릴리스 모드: $RELEASE_MODE"
echo ""

# ---- 클린 빌드 ----
if [ "$CLEAN_BUILD" = true ] && [ -d "$BUILD_DIR" ]; then
    echo "[1/4] 기존 빌드 디렉토리 삭제..."
    rm -rf "$BUILD_DIR"
fi

# ---- CMake 구성 ----
echo "[2/4] CMake 구성 중..."
CMAKE_EXTRA_FLAGS=""
if [ "$RELEASE_MODE" = true ]; then
    CMAKE_EXTRA_FLAGS="-DCMAKE_BUILD_TYPE=Release"
fi

emcmake cmake -B "$BUILD_DIR" "$PROJECT_ROOT" $CMAKE_EXTRA_FLAGS

# ---- 빌드 ----
echo "[3/4] 빌드 중..."
cmake --build "$BUILD_DIR" --target hongik-wasm -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ---- 출력 복사 ----
echo "[4/4] 빌드 결과물 복사..."
mkdir -p "$OUTPUT_DIR"

cp "$BUILD_DIR/hongik-wasm.js" "$OUTPUT_DIR/"
cp "$BUILD_DIR/hongik-wasm.wasm" "$OUTPUT_DIR/"

# 크기 정보 출력
echo ""
echo "=========================================="
echo " 빌드 완료"
echo "=========================================="
WASM_SIZE=$(wc -c < "$OUTPUT_DIR/hongik-wasm.wasm" 2>/dev/null || echo "?")
JS_SIZE=$(wc -c < "$OUTPUT_DIR/hongik-wasm.js" 2>/dev/null || echo "?")
echo "  hongik-wasm.wasm: ${WASM_SIZE} bytes"
echo "  hongik-wasm.js:   ${JS_SIZE} bytes"
echo ""
echo "출력 경로: $OUTPUT_DIR/"

# ---- 프론트엔드로 복사 (경로가 존재하면) ----
FRONTEND_WASM_DIR="$PROJECT_ROOT/../hongik-web/packages/wasm/public"
if [ -d "$PROJECT_ROOT/../hongik-web/packages/wasm" ]; then
    echo ""
    echo "프론트엔드 wasm 패키지로 복사 중..."
    mkdir -p "$FRONTEND_WASM_DIR"
    cp "$OUTPUT_DIR/hongik-wasm.js" "$FRONTEND_WASM_DIR/"
    cp "$OUTPUT_DIR/hongik-wasm.wasm" "$FRONTEND_WASM_DIR/"
    echo "  -> $FRONTEND_WASM_DIR/"
fi

echo ""
echo "완료!"
