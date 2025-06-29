#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "../object/object.h"
#include <map>
#include <string>

// Environment
// scope 내의 변수들을 저장하는 환경
class Environment {
public:
    std::map<std::string, Object*> store;
    Environment* outer;

    Object* Get(const std::string& name);

    Object* Set(const std::string& name, Object* object);
};

#endif // ENVIRONMENT_H
