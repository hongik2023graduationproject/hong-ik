#include "environment.h"

Object* Environment::Get(const std::string& name) {
    // TODO: 정의되지 않은 변수명을 호출하는 경우 적절한 에러 로그를 띄워주어야 한다.
    //       추가로 파이썬 인터프리터에서 작동하는 방식을 확인해볼 것
    Object* obj = store[name];
    if (obj == nullptr) {
        if (outer != nullptr) {
            return outer->Get(name);
        }
    }

    return obj;
}

Object* Environment::Set(const std::string& name, Object* object) {
    store[name] = object;
    return object;
}
