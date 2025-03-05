cmake_minimum_required(VERSION 3.30)
project(HongIk)

set(CMAKE_CXX_STANDARD 26)

add_executable(HongIk main.cpp
        repl/repl.cpp
        repl/repl.h
        utf8_converter/utf8_converter.cpp
        utf8_converter/utf8_converter.h
        lexer/lexer.cpp
        lexer/lexer.h
        token/token.h
        token/token.cpp
        token/token_type.h
        parser/Parser.cpp
        parser/Parser.h
        ast/program.h
        ast/node.h
        ast/statements.h
        ast/literals.h
        ast/expressions.h
        evaluator/Evaluator.cpp
        evaluator/Evaluator.h
        object/object.h
        object/object_type.h
        environment/environment.h
        environment/environment.cpp
)
