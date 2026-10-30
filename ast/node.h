#ifndef NODE_H
#define NODE_H

#include <string>

class Node {
public:
    virtual ~Node() = default;
    long long line = 0;  // 노드 대표(첫) 토큰의 줄 번호. 파서가 채우며 0이면 미상.
    // String 함수는 AST가 잘 생성되었는 지 디버깅 용도
    virtual std::string String() = 0;
};

#endif // NODE_H
