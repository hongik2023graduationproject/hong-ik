#include "hongik_error.h"
#include "../util/json_util.h"
#include <sstream>

static std::string errorTypeToString(HongIkErrorType type) {
    switch (type) {
    case HongIkErrorType::SYNTAX_ERROR: return "SyntaxError";
    case HongIkErrorType::RUNTIME_ERROR: return "RuntimeError";
    case HongIkErrorType::TYPE_ERROR: return "TypeError";
    case HongIkErrorType::REFERENCE_ERROR: return "ReferenceError";
    case HongIkErrorType::FILE_ERROR: return "FileError";
    default: return "UnknownError";
    }
}

std::string HongIkError::toJsonString() const {
    std::ostringstream os;
    os << "{";
    os << "\"type\":\"" << errorTypeToString(type) << "\",";
    os << "\"typeCode\":" << static_cast<int>(type) << ",";
    os << "\"message\":\"" << json_util::escape(message) << "\",";
    os << "\"location\":{";
    os << "\"line\":" << location.line << ",";
    os << "\"column\":" << location.column << ",";
    os << "\"endLine\":" << location.endLine << ",";
    os << "\"endColumn\":" << location.endColumn;
    os << "}";
    if (!code.empty()) {
        os << ",\"code\":\"" << json_util::escape(code) << "\"";
    }
    if (!suggestion.empty()) {
        os << ",\"suggestion\":\"" << json_util::escape(suggestion) << "\"";
    }
    os << "}";
    return os.str();
}
