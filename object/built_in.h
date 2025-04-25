#ifndef BUILT_IN_H
#define BUILT_IN_H

#include "object.h"


class Length : public Builtin {
    Object *function(std::vector<Object *> parameters) override;
};

class Print : public Builtin {
    Object *function(std::vector<Object *> parameters) override;
};

#endif //BUILT_IN_H
