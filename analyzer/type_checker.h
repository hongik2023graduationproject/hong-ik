#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "../ast/program.h"
#include "../token/token.h"
#include "../util/type.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// Phase A 정적 타입 검사기 (spec 2.1).
// 파서 결과(AST)에 대해 동작하는 별도 패스 — evaluator/VM/런타임 동작 변경 없음.

enum class Severity {
    WARNING,
    ERROR,
};

// CLI `--type-check=` 모드 (spec D4)
enum class TypeCheckMode {
    Off,
    Warn,    // 진단 출력 후 실행 계속 (Phase A 기본값)
    Strict,  // 파일 모드에서 진단 발생 시 실행 중단. REPL에서는 Warn으로 강등.
};

// 진단 위치·코드 (spec D5)
struct TypeDiagnostic {
    long long line;
    long long column;     // 가능하면, 없으면 0
    Severity severity;
    std::string code;     // "TC001"~ (spec D8)
    std::string message;  // 한글 메시지
};

class TypeChecker {
public:
    struct Result {
        std::vector<TypeDiagnostic> diagnostics;
        bool hasErrors() const;
    };

    TypeChecker();  // 빌트인 시그니처 prepopulate (spec D2.1)

    // 한 Program AST를 검사. 글로벌/클래스 스코프는 누적 보존됨 (REPL 다중 호출 대응).
    Result check(const std::shared_ptr<Program>& program);

    void clearDiagnostics();

private:
    struct Scope {
        std::map<std::string, std::shared_ptr<Type>> vars;
    };
    std::vector<Scope> scopes_;                                      // 함수/블록 스코프 push/pop
    std::map<std::string, std::shared_ptr<Type>> globalTypes_;       // 누적 보존
    std::map<std::string, std::shared_ptr<ClassType>> classTypes_;   // 누적 보존
    std::vector<TypeDiagnostic> diagnostics_;
    std::shared_ptr<Type> currentReturnType_;
    std::string currentFunctionName_;  // TC103 진단 메시지용
    long long currentLine_ = 0;  // 토큰 보유 노드 방문 시 갱신 (evaluator current_line 준용)

    void checkStatement(const std::shared_ptr<Statement>& stmt);
    void checkFunctionStatement(FunctionStatement& fn);
    std::shared_ptr<Type> inferExpression(const std::shared_ptr<Expression>& expr);
    std::shared_ptr<Type> inferCallExpression(CallExpression& call);

    void pushScope();
    void popScope();
    void declare(const std::string& name, std::shared_ptr<Type> type);
    std::shared_ptr<Type> lookup(const std::string& name);

    void error(long long line, const std::string& code, const std::string& msg);
    void warn(long long line, const std::string& code, const std::string& msg);
    void warnUnresolvedOptional(const OptionalType& opt);  // TC501 공통 발화

    // 타입 표기 토큰 → Type. 기본 키워드는 PrimType, IDENTIFIER는 classTypes_ lookup
    // (미등록 클래스 이름은 AnyType — Task 10에서 진단 처리).
    std::shared_ptr<Type> typeFromToken(const std::shared_ptr<Token>& tok, bool optional);

    void registerBuiltins();
};

#endif // TYPE_CHECKER_H
