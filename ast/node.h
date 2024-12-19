#ifndef NODE_H
#define NODE_H

#include <string>

class Node {
public:
    // String 함수는 AST가 잘 생성되었는 지 디버깅 용도
    virtual std::string String() = 0;
};


#endif //NODE_H
