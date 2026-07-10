#include "../built_in.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

// ===== I/O 내장함수 =====

std::shared_ptr<Object> Print::function(const std::vector<std::shared_ptr<Object>>& parameters) {
    string output;
    for (size_t i = 0; i < parameters.size(); i++) {
        if (auto str = dynamic_cast<String*>(parameters[i].get())) {
            output += str->value;
        } else if (auto integer = dynamic_cast<Integer*>(parameters[i].get())) {
            output += to_string(integer->value);
        } else if (auto float_obj = dynamic_cast<Float*>(parameters[i].get())) {
            output += float_obj->ToString();
        } else if (auto boolean = dynamic_cast<Boolean*>(parameters[i].get())) {
            output += (boolean->value ? "true" : "false");
        } else if (auto array = dynamic_cast<Array*>(parameters[i].get())) {
            output += array->ToString();
        } else if (auto hashmap = dynamic_cast<HashMap*>(parameters[i].get())) {
            output += hashmap->ToString();
        } else if (auto func = dynamic_cast<Function*>(parameters[i].get())) {
            output += func->ToString();
        } else if (dynamic_cast<Null*>(parameters[i].get())) {
            output += "없음";
        } else if (auto tuple = dynamic_cast<Tuple*>(parameters[i].get())) {
            output += tuple->ToString();
        } else if (auto inst = dynamic_cast<Instance*>(parameters[i].get())) {
            output += inst->ToString();
        } else {
            throw runtime_error("출력 함수에서 지원하지 않는 타입입니다.");
        }

        if (i < parameters.size() - 1) {
            output += " ";
        }
    }
    output += "\n";

    // IOContext가 있으면 사용, 없으면 기본 cout 사용
    if (ioCtx && ioCtx->print) {
        ioCtx->print(output);
    } else {
        cout << output;
    }

    return make_shared<Null>();
}

std::shared_ptr<Object> Input::function(const std::vector<std::shared_ptr<Object>>& parameters) {
    if (parameters.size() > 1) {
        throw runtime_error("입력 함수는 인자를 0개 또는 1개 받습니다.");
    }

    string prompt;
    if (parameters.size() == 1) {
        if (auto str = dynamic_cast<String*>(parameters[0].get())) {
            prompt = str->value;
        }
    }

    // IOContext가 있으면 사용, 없으면 기본 getline 사용
    string input;
    if (ioCtx && ioCtx->input) {
        input = ioCtx->input(prompt);
    } else {
        if (!prompt.empty()) {
            cout << prompt;
        }
        getline(cin, input);
    }
    return make_shared<String>(input);
}

std::shared_ptr<Object> FileRead::function(const std::vector<std::shared_ptr<Object>>& parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("파일읽기 함수는 인자를 1개 받습니다.");
    }

    auto* filename = dynamic_cast<String*>(parameters[0].get());
    if (!filename) {
        throw runtime_error("파일읽기 함수의 인자는 문자열(파일 경로)이어야 합니다.");
    }

    // IOContext가 있으면 사용, 없으면 기본 파일 I/O 사용
    if (ioCtx && ioCtx->fileRead) {
        string content = ioCtx->fileRead(filename->value);
        return make_shared<String>(content);
    }

    // 기본 파일 I/O
    // 경로 탐색 방지 (완전한 샌드박스는 아님)
    if (filename->value.find("..") != string::npos) {
        throw runtime_error("파일 경로에 '..'을 사용할 수 없습니다.");
    }
    if (!filename->value.empty() && (filename->value[0] == '/' || filename->value[0] == '\\')) {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
    }
    if (filename->value.size() >= 2 && filename->value[1] == ':') {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
    }

    ifstream file(filename->value);
    if (!file.is_open()) {
        throw runtime_error("파일을 열 수 없습니다: " + filename->value);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return make_shared<String>(buffer.str());
}

std::shared_ptr<Object> FileWrite::function(const std::vector<std::shared_ptr<Object>>& parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("파일쓰기 함수는 인자를 2개 받습니다 (파일 경로, 내용).");
    }

    auto* filename = dynamic_cast<String*>(parameters[0].get());
    if (!filename) {
        throw runtime_error("파일쓰기 함수의 첫 번째 인자는 문자열(파일 경로)이어야 합니다.");
    }
    auto* content = dynamic_cast<String*>(parameters[1].get());
    if (!content) {
        throw runtime_error("파일쓰기 함수의 두 번째 인자는 문자열(내용)이어야 합니다.");
    }

    // IOContext가 있으면 사용, 없으면 기본 파일 I/O 사용
    if (ioCtx && ioCtx->fileWrite) {
        ioCtx->fileWrite(filename->value, content->value);
        return make_shared<Null>();
    }

    // 기본 파일 I/O
    // 경로 탐색 방지 (완전한 샌드박스는 아님)
    if (filename->value.find("..") != string::npos) {
        throw runtime_error("파일 경로에 '..'을 사용할 수 없습니다.");
    }
    if (!filename->value.empty() && (filename->value[0] == '/' || filename->value[0] == '\\')) {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
    }
    if (filename->value.size() >= 2 && filename->value[1] == ':') {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
    }

    ofstream file(filename->value);
    if (!file.is_open()) {
        throw runtime_error("파일을 열 수 없습니다: " + filename->value);
    }

    file << content->value;
    return make_shared<Null>();
}
