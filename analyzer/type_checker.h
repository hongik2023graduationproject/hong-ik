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

    void checkStatement(const std::shared_ptr<Statement>& stmt);
    std::shared_ptr<Type> inferExpression(const std::shared_ptr<Expression>& expr);

    void pushScope();
    void popScope();
    void declare(const std::string& name, std::shared_ptr<Type> type);
    std::shared_ptr<Type> lookup(const std::string& name);

    void error(long long line, const std::string& code, const std::string& msg);
    void warn(long long line, const std::string& code, const std::string& msg);

    // 타입 표기 토큰 → Type. 기본 키워드는 PrimType, IDENTIFIER는 classTypes_ lookup
    // (미등록 클래스 이름은 AnyType — Task 10에서 진단 처리).
    std::shared_ptr<Type> typeFromToken(const std::shared_ptr<Token>& tok, bool optional);

    void registerBuiltins();
};

#endif // TYPE_CHECKER_H
