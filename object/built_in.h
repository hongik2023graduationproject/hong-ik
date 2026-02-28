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

#endif // BUILT_IN_H
