#ifndef HONGIK_TOKEN_UTILS_H
#define HONGIK_TOKEN_UTILS_H

#include "../token/token.h"
#include "../token/token_type.h"
#include <memory>
#include <vector>

namespace token_utils {

/**
 * Append synthetic END_BLOCK tokens for any START_BLOCKs left open by the lexer.
 *
 * The hongik grammar uses indentation-driven blocks, but the lexer emits
 * END_BLOCK only when it sees a dedent. A file (or REPL chunk) that ends
 * inside a block leaves START_BLOCKs unmatched. Both the WASM entry point
 * and the desktop REPL/file pipelines need to balance them before parsing,
 * so that logic lives here once.
 */
inline void appendMissingBlockClosers(std::vector<std::shared_ptr<Token>>& tokens) {
    int depth = 0;
    for (const auto& t : tokens) {
        if (t->type == TokenType::START_BLOCK) ++depth;
        else if (t->type == TokenType::END_BLOCK) --depth;
    }
    for (int i = 0; i < depth; ++i) {
        tokens.push_back(std::make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
    }
}

} // namespace token_utils

#endif // HONGIK_TOKEN_UTILS_H
