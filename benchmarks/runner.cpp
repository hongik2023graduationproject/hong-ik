#include "runner.h"
#include "evaluator/evaluator.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "utf8_converter/utf8_converter.h"
#include "util/token_utils.h"
#include "vm/compiler.h"
#include "vm/vm.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace bench {

std::string loadScenarioSource(const std::string& name) {
    std::string path = std::string(HONGIK_BENCH_SCENARIO_DIR) + "/" + name + ".hik";
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("시나리오 파일을 열 수 없음: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::shared_ptr<Program> parseOnly(const std::string& source) {
    std::string src = source;
    if (src.empty() || src.back() != '\n')
        src += '\n';
    Lexer lexer;
    auto tokens = lexer.Tokenize(Utf8Converter::Convert(src));
    token_utils::appendMissingBlockClosers(tokens);
    Parser parser;
    auto program = parser.Parsing(tokens);
    if (!parser.getErrors().empty())
        throw std::runtime_error("시나리오 파스 에러: " + parser.getErrors().front());
    return program;
}

ScenarioProgram prepare(const std::string& source) {
    ScenarioProgram sp;
    sp.program = parseOnly(source);
    Compiler compiler;
    sp.bytecode = compiler.Compile(sp.program);
    return sp;
}

ScenarioProgram loadScenario(const std::string& name) {
    return prepare(loadScenarioSource(name));
}

std::shared_ptr<Object> runVm(const ScenarioProgram& sp) {
    VM vm;
    return vm.Execute(sp.bytecode);
}

std::shared_ptr<Object> runEval(const ScenarioProgram& sp) {
    Evaluator evaluator;
    return evaluator.Evaluate(sp.program);
}

std::string buildFrontendSource(int repeats) {
    // 함수·클래스·루프·조건이 섞인 대표 블록 — 프론트엔드가 실제로 만나는 코드 분포 근사
    static const std::string kBlock = "함수 계산기(정수 a, 정수 b) -> 정수:\n"
                                      "    정수 결과 = a * b + a - b\n"
                                      "    만약 결과 > 100 라면:\n"
                                      "        리턴 결과 % 100\n"
                                      "    리턴 결과\n"
                                      "클래스 저장소:\n"
                                      "    정수 값\n"
                                      "    생성(정수 초기):\n"
                                      "        자기.값 = 초기\n"
                                      "    함수 읽기() -> 정수:\n"
                                      "        리턴 자기.값\n"
                                      "정수 누적 = 0\n"
                                      "정수 n = 0\n"
                                      "반복 n < 10 동안:\n"
                                      "    누적 += 계산기(n, n + 1)\n"
                                      "    n += 1\n";
    std::string out;
    out.reserve(kBlock.size() * repeats);
    for (int i = 0; i < repeats; i++)
        out += kBlock;
    return out;
}

} // namespace bench
