#include "analyzer/type_checker.h"
#include "lexer/lexer.h"
#include "object/builtin_registry.h"
#include "object/builtin_signatures.h"
#include "parser/parser.h"
#include "utf8_converter/utf8_converter.h"
#include "util/token_utils.h"
#include "util/type.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <unordered_set>

namespace {

// 소스 문자열 → AST → TypeChecker 진단. 파일 모드와 동일하게 줄 단위 토크나이즈
// 후 블록 클로저 보정 (repl.cpp FileMode 파이프라인 준용).
TypeChecker::Result checkSource(TypeChecker& tc, const std::string& source) {
    Lexer lexer;
    std::vector<std::shared_ptr<Token>> tokens;
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        line += "\n";
        auto utf8 = Utf8Converter::Convert(line);
        auto newTokens = lexer.Tokenize(utf8);
        tokens.insert(tokens.end(), newTokens.begin(), newTokens.end());
    }
    token_utils::appendMissingBlockClosers(tokens);
    Parser parser;
    auto program = parser.Parsing(tokens);
    if (!parser.getErrors().empty()) {
        ADD_FAILURE() << "parse error: " << parser.getErrors().front();
    }
    return tc.check(program);
}

// 단일 진단 검증 헬퍼
void expectSingleDiagnostic(const TypeChecker::Result& result, const std::string& code) {
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, code);
}

} // namespace

// Phase A 정적 타입 시스템 — Type 계층 단위 테스트
// spec: docs/superpowers/specs/2026-05-19-static-type-system-design.md (1.1)

TEST(TypeTest, AssignabilityRules) {
    auto integer = makePrim(ObjectType::INTEGER);
    auto floating = makePrim(ObjectType::FLOAT);
    auto str = makePrim(ObjectType::STRING);
    auto nullObj = makePrim(ObjectType::NULL_OBJ);
    auto optInteger = makeOptional(makePrim(ObjectType::INTEGER));
    auto any = makeAny();
    auto never = makeNever();

    // T <- T 동일 OK
    EXPECT_TRUE(integer->isAssignableFrom(*integer));

    // T <- U (다른 Prim) 거부
    EXPECT_FALSE(integer->isAssignableFrom(*str));

    // 실수 <- 정수 자동 승격 거부 (spec 1.1, Task 0 결과 박제)
    EXPECT_FALSE(floating->isAssignableFrom(*integer));

    // T? <- T 좁히기 OK
    EXPECT_TRUE(optInteger->isAssignableFrom(*integer));

    // T? <- 없음 OK
    EXPECT_TRUE(optInteger->isAssignableFrom(*nullObj));

    // T <- 없음 거부
    EXPECT_FALSE(integer->isAssignableFrom(*nullObj));

    // any <- * / * <- any 항상 OK
    EXPECT_TRUE(any->isAssignableFrom(*integer));
    EXPECT_TRUE(any->isAssignableFrom(*optInteger));
    EXPECT_TRUE(integer->isAssignableFrom(*any));
    EXPECT_TRUE(optInteger->isAssignableFrom(*any));

    // never 양방향 통과 (cascade 차단)
    EXPECT_TRUE(never->isAssignableFrom(*integer));
    EXPECT_TRUE(integer->isAssignableFrom(*never));
    EXPECT_TRUE(optInteger->isAssignableFrom(*never));
}

// plan Task 3 게이트: 시그니처 테이블 ↔ BuiltinRegistry 1:1 매칭.
// `반복`은 lexer 키워드 충돌로 호출 불가능한 dead builtin — 레지스트리에는
// 있지만 시그니처 테이블에서는 의도적으로 제외 (spec D2.1).
TEST(BuiltinSignaturesTest, MatchesRegistry) {
    const auto& registryNames = BuiltinRegistry::names();

    std::unordered_set<std::string> tableNames;
    for (const auto& sig : kBuiltinSignatures) {
        EXPECT_TRUE(tableNames.insert(sig.name).second) << "시그니처 중복: " << sig.name;
        EXPECT_TRUE(registryNames.count(sig.name)) << "레지스트리에 없는 시그니처: " << sig.name;
        EXPECT_LE(sig.minArity, sig.maxArity) << sig.name;
        EXPECT_GE(sig.minArity, 0) << sig.name;
    }

    for (const auto& name : registryNames) {
        if (name == "반복") continue;  // dead builtin (spec D2.1)
        EXPECT_TRUE(tableNames.count(name)) << "시그니처 누락: " << name;
    }

    EXPECT_EQ(tableNames.size(), registryNames.size() - 1);  // 반복 1건 제외
}

// plan Task 4 게이트
TEST(TypeCheckerTest, EmptyProgram) {
    TypeChecker tc;
    auto result = tc.check(std::make_shared<Program>());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_FALSE(result.hasErrors());
}

TEST(TypeCheckerTest, BuiltinResolved) {
    TypeChecker tc;
    auto result = checkSource(tc, "출력(\"안녕\")\n");
    EXPECT_TRUE(result.diagnostics.empty());  // 빌트인 prepopulate로 TC006 미발화
}

// ---- plan Task 6: TC001/TC002 + Z2 AST 노드 6종 ----

TEST(TypeCheckerTest, TC001_DeclTypeMismatch) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수 x = \"hello\"\n"), "TC001");
}

TEST(TypeCheckerTest, TC001_DeclTypeMatch) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "정수 x = 10\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC001_OptionalAcceptsNull) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "정수? z = 없음\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC001_NonNullRejectsNull) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수 y = 없음\n"), "TC001");
}

TEST(TypeCheckerTest, TC001_EmptyArray) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "배열 a = []\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC001_MixedArrayFlat) {
    TypeChecker tc;
    // Phase A: 원소 타입 무시, 평면 ARRAY 처리 (spec D2.2)
    EXPECT_TRUE(checkSource(tc, "배열 a = [1, \"x\"]\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC002_ReassignMismatch) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수 x = 1\nx = \"a\"\n"), "TC002");
}

TEST(TypeCheckerTest, TC002_ReassignMatch) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "정수 x = 1\nx = 2\n").diagnostics.empty());
}

TEST(TypeCheckerTest, Z2_ForEachNoFalsePositive) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "각각 정수 원소 [1, 2, 3] 에서:\n"
        "    출력(원소)\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, Z2_ForRangeNoFalsePositive) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "반복 정수 i = 0 부터 10 까지:\n"
        "    출력(i)\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, Z2_TryCatchNoFalsePositive) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "시도:\n"
        "    정수 x = 1 / 0\n"
        "실패 오류:\n"
        "    출력(오류)\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, Z2_MatchNoFalsePositive) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "정수 x = 1\n"
        "비교 x:\n"
        "    경우 1:\n"
        "        출력(\"일\")\n"
        "    기본:\n"
        "        출력(\"기타\")\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, Z2_IndexAssignmentNoCheck) {
    TypeChecker tc;
    // IndexAssignment는 타입 일치 검사 안 함 (spec D6.5)
    auto result = checkSource(tc,
        "배열 a = [1, 2, 3]\n"
        "a[0] = \"x\"\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

// ---- plan Task 7: TC006 미선언 식별자 + 식별자 추론 ----

TEST(TypeCheckerTest, TC006_UndeclaredIdentifier) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수 x = foo\n"), "TC006");
}

TEST(TypeCheckerTest, TC006_UndeclaredInBinaryNoCascade) {
    TypeChecker tc;
    // NeverType 전파로 이항 연산·대입에서 추가 진단 없음 (spec 1.1.2)
    expectSingleDiagnostic(checkSource(tc, "정수 x = 미선언 + 1\n"), "TC006");
}

TEST(TypeCheckerTest, IdentifierInfersDeclaredType) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수 x = 1\n문자 s = x\n"), "TC001");
}

TEST(TypeCheckerTest, IdentifierMatchingTypeOk) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "정수 x = 1\n정수 y = x\n").diagnostics.empty());
}

// ---- plan Task 8: TC101/TC102/TC103 함수 호출/리턴 (top-down) ----

TEST(TypeCheckerTest, TC102_ArgTypeMismatch) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "함수 f(정수 a) -> 정수:\n"
        "    리턴 a\n"
        "f(\"x\")\n");
    expectSingleDiagnostic(result, "TC102");
}

TEST(TypeCheckerTest, TC101_ArgCountMismatch) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "함수 f(정수 a) -> 정수:\n"
        "    리턴 a\n"
        "f()\n");
    expectSingleDiagnostic(result, "TC101");
}

TEST(TypeCheckerTest, TC103_ReturnTypeMismatch) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "함수 f() -> 정수:\n"
        "    리턴 \"a\"\n");
    expectSingleDiagnostic(result, "TC103");
}

TEST(TypeCheckerTest, FunctionCallOk) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "함수 f() -> 정수:\n"
        "    리턴 1\n"
        "정수 x = f()\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, RecursionOk) {
    TypeChecker tc;
    // 본문 진입 직전 declare → 재귀 호출 가능 (spec 2.3)
    auto result = checkSource(tc,
        "함수 사실() -> 정수:\n"
        "    리턴 사실()\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, ForwardReferenceIsTC006) {
    TypeChecker tc;
    // hoisting 미지원 (spec D3.1) — evaluator도 런타임 실패하는 케이스
    auto result = checkSource(tc,
        "정수 x = 나중에()\n"
        "함수 나중에() -> 정수:\n"
        "    리턴 1\n");
    expectSingleDiagnostic(result, "TC006");
}

TEST(TypeCheckerTest, UndeclaredFunctionCallSingleTC006) {
    TypeChecker tc;
    // NeverType 차단으로 TC101/TC102 추가 발화 없음
    expectSingleDiagnostic(checkSource(tc, "없는함수(1)\n"), "TC006");
}

TEST(TypeCheckerTest, TC101_BuiltinArity) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "길이()\n"), "TC101");
}

TEST(TypeCheckerTest, HigherOrderSpecialOpsResolved) {
    TypeChecker tc;
    // 매핑/걸러내기/줄이기는 빌트인 맵에 없는 특수 연산자 — TC006 미발화
    auto result = checkSource(tc,
        "함수 두배(정수 x) -> 정수:\n"
        "    리턴 x * 2\n"
        "배열 a = [1, 2, 3]\n"
        "배열 b = 매핑(a, 두배)\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

// ---- plan Task 9: TC501 Nullable 미해제 ----

TEST(TypeCheckerTest, TC501_BinaryOnOptional) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수? x = 10\nx + 1\n"), "TC501");
}

TEST(TypeCheckerTest, TC501_EqualityExempt) {
    TypeChecker tc;
    // null 체크 패턴 `x == 없음`은 예외 (spec 3 TC501)
    auto result = checkSource(tc, "정수? x = 10\n논리 b = x == 없음\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC501_IndexOnOptional) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "배열? a = [1, 2]\na[0]\n"), "TC501");
}

TEST(TypeCheckerTest, TC501_OptionalToNonNullIsTC001) {
    TypeChecker tc;
    // 대입 검사가 먼저 — TC501 아님 (plan Task 9)
    expectSingleDiagnostic(checkSource(tc, "정수? x = 10\n정수 y = x\n"), "TC001");
}

TEST(TypeCheckerTest, TC501_NoNarrowingInPhaseA) {
    TypeChecker tc;
    // 좁히기 미구현 (Phase B) — null 검사 분기 안에서도 TC501 발화
    auto result = checkSource(tc,
        "정수? x = 10\n"
        "만약 x != 없음 라면:\n"
        "    정수 y = x + 1\n");
    expectSingleDiagnostic(result, "TC501");
}

// ---- plan Task 10: 클래스 처리 (spec D6) ----

namespace {
const char* kPointClass =
    "클래스 점:\n"
    "    정수 x\n"
    "    정수 y\n"
    "    생성(정수 a, 정수 b):\n"
    "        자기.x = a\n"
    "        자기.y = b\n"
    "    함수 합() -> 정수:\n"
    "        리턴 자기.x + 자기.y\n";
} // namespace

TEST(TypeCheckerTest, ClassConstructorOk) {
    TypeChecker tc;
    auto result = checkSource(tc, std::string(kPointClass) + "점 p = 점(3, 4)\n출력(p.x)\n출력(p.합())\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC101_ConstructorArity) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, std::string(kPointClass) + "점 p = 점(3)\n"), "TC101");
}

TEST(TypeCheckerTest, MethodBodyNotTraversed) {
    TypeChecker tc;
    // Phase A: 클래스 본문 미진입 — 메서드 내부의 타입 문제는 진단하지 않음 (spec D6)
    auto result = checkSource(tc,
        "클래스 동물:\n"
        "    문자 이름\n"
        "    생성(문자 이름):\n"
        "        자기.이름 = 이름\n"
        "    함수 인사() -> 정수:\n"
        "        리턴 자기.이름 + 1\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC006_UndeclaredParentClass) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 강아지 < 모름:\n"
        "    정수 나이\n");
    expectSingleDiagnostic(result, "TC006");
}

TEST(TypeCheckerTest, InheritanceDeclaredParentOk) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 동물:\n"
        "    문자 이름\n"
        "클래스 강아지 < 동물:\n"
        "    정수 나이\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC501_MemberAccessOnOptionalClass) {
    TypeChecker tc;
    auto result = checkSource(tc, std::string(kPointClass) + "점? p = 없음\np.x\n");
    expectSingleDiagnostic(result, "TC501");
}

// ---- plan Task 12: 종합 보강 ----

TEST(TypeCheckerTest, TC102_ArgTypeMatchOk) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "함수 f(정수 a) -> 정수:\n"
        "    리턴 a\n"
        "정수 x = f(10)\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC002_OptionalReassignNullOk) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "정수? x = 10\nx = 없음\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC002_NonNullReassignNullRejected) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수 x = 10\nx = 없음\n"), "TC002");
}

TEST(TypeCheckerTest, Z2_YieldNoFalsePositive) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "함수 숫자들() -> 정수:\n"
        "    생산 1\n"
        "    생산 2\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC103_ReturnOkSecondCase) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "함수 g(실수 v) -> 실수:\n"
        "    리턴 v\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, ReplAccumulatesGlobalsAcrossChecks) {
    TypeChecker tc;
    // REPL 시나리오: 같은 인스턴스로 두 번 check — 첫 입력의 선언이 둘째에 보임
    EXPECT_TRUE(checkSource(tc, "정수 x = 1\n").diagnostics.empty());
    expectSingleDiagnostic(checkSource(tc, "x = \"a\"\n"), "TC002");
}

TEST(TypeCheckerTest, ScopePopAfterForEach) {
    TypeChecker tc;
    // 루프 변수는 루프 스코프에만 존재 — 바깥 재선언과 충돌하지 않아야 함
    auto result = checkSource(tc,
        "각각 정수 원소 [1, 2, 3] 에서:\n"
        "    출력(원소)\n"
        "문자 원소 = \"바깥\"\n");
    EXPECT_TRUE(result.diagnostics.empty());
}
