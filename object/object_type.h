#ifndef OBJECT_TYPE_H
#define OBJECT_TYPE_H

enum class ObjectType {
    INTEGER,
    FLOAT,
    BOOLEAN,
    STRING,
    RETURN_VALUE,
    FUNCTION,
    BUILTIN_FUNCTION,
    ARRAY,
    HASH_MAP,
    BREAK_SIGNAL,
    NULL_OBJ,
    TUPLE,
    CLASS_DEF,
    INSTANCE,
};

#endif // OBJECT_TYPE_H
