#ifndef BUILT_IN_H
#define BUILT_IN_H

#include "object.h"


class Length : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Print : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Push : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class TypeOf : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class ToInteger : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class ToFloat : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class ToString_ : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Input : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Keys : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Contains : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class MapSet : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Remove : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class FileRead : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class FileWrite : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

// 수학 내장함수
class Abs : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Sqrt : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Max : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Min : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Random : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

// 문자열 내장함수
class Split : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class ToUpper : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class ToLower : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Replace : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Trim : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

// 배열 내장함수
class Sort : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Reverse : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Find : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

class Slice : public Builtin {
    std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> parameters) override;
};

#endif // BUILT_IN_H
