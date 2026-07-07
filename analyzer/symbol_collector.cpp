#include "symbol_collector.h"

namespace {

SymbolInfo makeInfo(std::string name, DocSymbolKind kind, std::string typeText, std::string container,
                    const std::shared_ptr<Token>& nameToken, long long fallbackLine) {
    SymbolInfo info;
    info.name = std::move(name);
    info.kind = kind;
    info.typeText = std::move(typeText);
    info.container = std::move(container);
    if (nameToken) {
        info.line = nameToken->line;
        info.column = nameToken->column;
        info.endColumn = nameToken->endColumn;
    } else {
        info.line = fallbackLine;
    }
    return info;
}

std::string functionSignature(const FunctionStatement& fn) {
    std::string s = "함수 " + fn.name + "(";
    for (size_t i = 0; i < fn.parameters.size(); ++i) {
        if (i) s += ", ";
        if (i < fn.parameterTypes.size() && fn.parameterTypes[i]) {
            s += fn.parameterTypes[i]->text;
            if (i < fn.parameterOptionals.size() && fn.parameterOptionals[i]) s += "?";
            s += " ";
        }
        s += fn.parameters[i]->name;
    }
    s += ")";
    if (fn.returnType) {
        s += " -> " + fn.returnType->text;
        if (fn.returnTypeOptional) s += "?";
    }
    return s;
}

} // namespace

std::vector<SymbolInfo> SymbolCollector::collect(const std::shared_ptr<Program>& program) {
    std::vector<SymbolInfo> out;
    if (!program) return out;
    for (const auto& stmt : program->statements) walkStatement(stmt, "", out);
    return out;
}

void SymbolCollector::walkBlock(const std::shared_ptr<BlockStatement>& block, const std::string& container,
                                std::vector<SymbolInfo>& out) {
    if (!block) return;
    for (const auto& stmt : block->statements) walkStatement(stmt, container, out);
}

void SymbolCollector::addFunction(const std::shared_ptr<FunctionStatement>& fn, const std::string& container,
                                  DocSymbolKind kind, std::vector<SymbolInfo>& out) {
    out.push_back(makeInfo(fn->name, kind, functionSignature(*fn), container, fn->nameToken, fn->line));
    for (size_t i = 0; i < fn->parameters.size(); ++i) {
        const auto& p = fn->parameters[i];
        std::string typeText = (i < fn->parameterTypes.size() && fn->parameterTypes[i]) ? fn->parameterTypes[i]->text : "";
        if (i < fn->parameterOptionals.size() && fn->parameterOptionals[i]) typeText += "?";
        out.push_back(makeInfo(p->name, DocSymbolKind::Parameter, typeText, fn->name, p->token, fn->line));
    }
    walkBlock(fn->body, fn->name, out);
}

void SymbolCollector::walkStatement(const std::shared_ptr<Statement>& stmt, const std::string& container,
                                    std::vector<SymbolInfo>& out) {
    if (!stmt) return;
    if (auto init = std::dynamic_pointer_cast<InitializationStatement>(stmt)) {
        std::string typeText = init->type ? init->type->text : "";
        if (init->isOptional) typeText += "?";
        out.push_back(makeInfo(init->name, DocSymbolKind::Variable, typeText, container, init->nameToken, init->line));
    } else if (auto fn = std::dynamic_pointer_cast<FunctionStatement>(stmt)) {
        addFunction(fn, container, DocSymbolKind::Function, out);
    } else if (auto cls = std::dynamic_pointer_cast<ClassStatement>(stmt)) {
        out.push_back(makeInfo(cls->name, DocSymbolKind::Class, "클래스 " + cls->name, container, cls->nameToken, cls->line));
        for (size_t i = 0; i < cls->fieldNames.size(); ++i) {
            std::string typeText = (i < cls->fieldTypes.size() && cls->fieldTypes[i]) ? cls->fieldTypes[i]->text : "";
            auto tok = i < cls->fieldNameTokens.size() ? cls->fieldNameTokens[i] : nullptr;
            out.push_back(makeInfo(cls->fieldNames[i], DocSymbolKind::Field, typeText, cls->name, tok, cls->line));
        }
        for (size_t i = 0; i < cls->constructorParams.size(); ++i) {
            const auto& p = cls->constructorParams[i];
            std::string typeText =
                (i < cls->constructorParamTypes.size() && cls->constructorParamTypes[i]) ? cls->constructorParamTypes[i]->text : "";
            out.push_back(makeInfo(p->name, DocSymbolKind::Parameter, typeText, cls->name, p->token, cls->line));
        }
        walkBlock(cls->constructorBody, cls->name, out);
        for (const auto& m : cls->methods) addFunction(m, cls->name, DocSymbolKind::Method, out);
    } else if (auto fe = std::dynamic_pointer_cast<ForEachStatement>(stmt)) {
        out.push_back(makeInfo(fe->elementName, DocSymbolKind::Variable,
                               fe->elementType ? fe->elementType->text : "", container, fe->nameToken, fe->line));
        walkBlock(fe->body, container, out);
    } else if (auto fr = std::dynamic_pointer_cast<ForRangeStatement>(stmt)) {
        out.push_back(makeInfo(fr->varName, DocSymbolKind::Variable, fr->varType ? fr->varType->text : "", container,
                               fr->nameToken, fr->line));
        walkBlock(fr->body, container, out);
    } else if (auto iff = std::dynamic_pointer_cast<IfStatement>(stmt)) {
        walkBlock(iff->consequence, container, out);
        walkBlock(iff->then, container, out);
    } else if (auto wh = std::dynamic_pointer_cast<WhileStatement>(stmt)) {
        walkBlock(wh->body, container, out);
    } else if (auto tc = std::dynamic_pointer_cast<TryCatchStatement>(stmt)) {
        walkBlock(tc->tryBody, container, out);
        walkBlock(tc->catchBody, container, out);
    } else if (auto mt = std::dynamic_pointer_cast<MatchStatement>(stmt)) {
        for (const auto& b : mt->caseBodies) walkBlock(b, container, out);
        walkBlock(mt->defaultBody, container, out);
    } else if (auto blk = std::dynamic_pointer_cast<BlockStatement>(stmt)) {
        walkBlock(blk, container, out);
    }
    // Assignment/Expression/Return/Import 등은 선언이 아님 — 무시
}
