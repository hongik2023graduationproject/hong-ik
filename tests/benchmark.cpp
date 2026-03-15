#include "lexer/lexer.h"
#include "parser/parser.h"
#include "evaluator/evaluator.h"
#include "vm/compiler.h"
#include "vm/vm.h"
#include "utf8_converter/utf8_converter.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

// 소스코드 -> 토큰 -> AST 공통 처리
shared_ptr<Program> parseSource(const string& source) {
    Lexer lexer;
    vector<shared_ptr<Token>> allTokens;
    istringstream stream(source);
    string line;
    int indent = 0;

    while (getline(stream, line)) {
        line += "\n";
        auto utf8 = Utf8Converter::Convert(line);
        auto tokens = lexer.Tokenize(utf8);
        allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());
        for (auto& t : tokens) {
            if (t->type == TokenType::START_BLOCK) indent++;
            if (t->type == TokenType::END_BLOCK) indent--;
        }
    }
    for (int i = 0; i < indent; i++) {
        allTokens.push_back(make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
    }

    Parser parser;
    return parser.Parsing(allTokens);
}

int main() {
    // 벤치마크 1: 반복문 산술 (1부터 10000까지 합)
    string loopBench =
        "정수 합계 = 0\n"
        "정수 i = 1\n"
        "반복 i <= 10000 동안:\n"
        "    합계 += i\n"
        "    i += 1\n"
        "\n"
        "합계\n";

    // 벤치마크 2: 함수 호출 반복
    string funcBench =
        "함수 더하기(정수 a, 정수 b) -> 정수:\n"
        "    리턴 a + b\n"
        "\n"
        "정수 결과 = 0\n"
        "정수 i = 0\n"
        "반복 i < 5000 동안:\n"
        "    결과 = 더하기(결과, i)\n"
        "    i += 1\n"
        "\n"
        "결과\n";

    // 벤치마크 3: 피보나치 재귀
    string fibBench =
        "함수 피보나치(정수 수) -> 정수:\n"
        "    만약 수 <= 1 라면:\n"
        "        리턴 수\n"
        "    리턴 피보나치(수 - 1) + 피보나치(수 - 2)\n"
        "\n"
        "피보나치(20)\n";

    struct Benchmark {
        string name;
        string source;
    };

    vector<Benchmark> benchmarks = {
        {"반복문 합계 (1~10000)", loopBench},
        {"함수 호출 반복 (5000회)", funcBench},
        {"피보나치 재귀 (20)", fibBench},
    };

    cout << "=== hong-ik 성능 벤치마크 ===" << endl;
    cout << endl;

    for (auto& bench : benchmarks) {
        auto program = parseSource(bench.source);

        // 트리워킹 인터프리터
        const int RUNS = 3;
        double evalTotal = 0;
        shared_ptr<Object> evalResult;

        for (int r = 0; r < RUNS; r++) {
            auto program2 = parseSource(bench.source); // 매번 새 파싱 (evaluator가 환경을 수정하므로)
            Evaluator evaluator;
            auto start = chrono::high_resolution_clock::now();
            evalResult = evaluator.Evaluate(program2);
            auto end = chrono::high_resolution_clock::now();
            evalTotal += chrono::duration<double, milli>(end - start).count();
        }
        double evalAvg = evalTotal / RUNS;

        // 바이트코드 VM
        double vmTotal = 0;
        shared_ptr<Object> vmResult;

        for (int r = 0; r < RUNS; r++) {
            auto program2 = parseSource(bench.source);
            Compiler compiler;
            auto bytecode = compiler.Compile(program2);
            VM vm;
            auto start = chrono::high_resolution_clock::now();
            vmResult = vm.Execute(bytecode);
            auto end = chrono::high_resolution_clock::now();
            vmTotal += chrono::duration<double, milli>(end - start).count();
        }
        double vmAvg = vmTotal / RUNS;

        double speedup = evalAvg / vmAvg;

        cout << "[" << bench.name << "]" << endl;
        cout << "  트리워킹: " << evalAvg << " ms (결과: " << (evalResult ? evalResult->ToString() : "null") << ")" << endl;
        cout << "  바이트코드 VM: " << vmAvg << " ms (결과: " << (vmResult ? vmResult->ToString() : "null") << ")" << endl;
        cout << "  속도 비율: " << speedup << "x" << endl;
        cout << endl;
    }

    return 0;
}
