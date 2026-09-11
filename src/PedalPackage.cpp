#include "PedalPackage.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace circuitpedal {
namespace {

struct JsonValue {
    enum class Type { Null, Boolean, Number, String, Object, Array };

    Type type = Type::Null;
    bool booleanValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::map<std::string, JsonValue> objectValue;
    std::vector<JsonValue> arrayValue;
};

class JsonParser {
public:
    explicit JsonParser(std::string text)
        : text_(std::move(text))
    {
    }

    bool parse(JsonValue& value, std::string& error)
    {
        skipWhitespace();
        if (!parseValue(value, error))
            return false;
        skipWhitespace();
        if (position_ != text_.size())
        {
            error = errorAt("unexpected trailing JSON data");
            return false;
        }
        return true;
    }

private:
    bool parseValue(JsonValue& value, std::string& error)
    {
        skipWhitespace();
        if (position_ >= text_.size())
        {
            error = errorAt("unexpected end of JSON");
            return false;
        }

        const char c = text_[position_];
        if (c == '{')
            return parseObject(value, error);
        if (c == '[')
            return parseArray(value, error);
        if (c == '"')
        {
            value.type = JsonValue::Type::String;
            return parseString(value.stringValue, error);
        }
        if (c == 't')
            return parseLiteral("true", JsonValue::Type::Boolean, value, error, true);
        if (c == 'f')
            return parseLiteral("false", JsonValue::Type::Boolean, value, error, false);
        if (c == 'n')
            return parseLiteral("null", JsonValue::Type::Null, value, error, false);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0)
            return parseNumber(value, error);

        error = errorAt("unexpected character while reading JSON value");
        return false;
    }

    bool parseObject(JsonValue& value, std::string& error)
    {
        ++position_;
        value = JsonValue {};
        value.type = JsonValue::Type::Object;
        skipWhitespace();
        if (consume('}'))
            return true;

        while (position_ < text_.size())
        {
            std::string key;
            if (!parseString(key, error))
                return false;
            skipWhitespace();
            if (!consume(':'))
            {
                error = errorAt("expected ':' after object key");
                return false;
            }

            JsonValue child;
            if (!parseValue(child, error))
                return false;
            value.objectValue[key] = std::move(child);

            skipWhitespace();
            if (consume('}'))
                return true;
            if (!consume(','))
            {
                error = errorAt("expected ',' or '}' in object");
                return false;
            }
            skipWhitespace();
        }

        error = errorAt("unterminated JSON object");
        return false;
    }

    bool parseArray(JsonValue& value, std::string& error)
    {
        ++position_;
        value = JsonValue {};
        value.type = JsonValue::Type::Array;
        skipWhitespace();
        if (consume(']'))
            return true;

        while (position_ < text_.size())
        {
            JsonValue child;
            if (!parseValue(child, error))
                return false;
            value.arrayValue.push_back(std::move(child));

            skipWhitespace();
            if (consume(']'))
                return true;
            if (!consume(','))
            {
                error = errorAt("expected ',' or ']' in array");
                return false;
            }
            skipWhitespace();
        }

        error = errorAt("unterminated JSON array");
        return false;
    }

    bool parseString(std::string& output, std::string& error)
    {
        if (!consume('"'))
        {
            error = errorAt("expected JSON string");
            return false;
        }

        output.clear();
        while (position_ < text_.size())
        {
            const char c = text_[position_++];
            if (c == '"')
                return true;
            if (static_cast<unsigned char>(c) < 0x20U)
            {
                error = errorAt("control character in JSON string");
                return false;
            }
            if (c != '\\')
            {
                output.push_back(c);
                continue;
            }

            if (position_ >= text_.size())
            {
                error = errorAt("unterminated escape sequence");
                return false;
            }

            const char escaped = text_[position_++];
            switch (escaped)
            {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u':
                    // Manifests are UTF-8 files. Keep the parser compact: accept
                    // ASCII \u00XX escapes and require other Unicode as UTF-8.
                    if (position_ + 4U > text_.size())
                    {
                        error = errorAt("incomplete Unicode escape");
                        return false;
                    }
                    {
                        unsigned value = 0U;
                        for (int i = 0; i < 4; ++i)
                        {
                            const char h = text_[position_++];
                            value <<= 4U;
                            if (h >= '0' && h <= '9') value |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') value |= static_cast<unsigned>(10 + h - 'a');
                            else if (h >= 'A' && h <= 'F') value |= static_cast<unsigned>(10 + h - 'A');
                            else
                            {
                                error = errorAt("invalid Unicode escape");
                                return false;
                            }
                        }
                        if (value > 0x7FU)
                        {
                            error = errorAt("use UTF-8 directly for non-ASCII manifest text");
                            return false;
                        }
                        output.push_back(static_cast<char>(value));
                    }
                    break;
                default:
                    error = errorAt("unsupported JSON escape sequence");
                    return false;
            }
        }

        error = errorAt("unterminated JSON string");
        return false;
    }

    bool parseNumber(JsonValue& value, std::string& error)
    {
        const std::size_t start = position_;
        if (text_[position_] == '-')
            ++position_;

        if (position_ >= text_.size())
        {
            error = errorAt("incomplete JSON number");
            return false;
        }

        if (text_[position_] == '0')
        {
            ++position_;
        }
        else
        {
            if (std::isdigit(static_cast<unsigned char>(text_[position_])) == 0)
            {
                error = errorAt("invalid JSON number");
                return false;
            }
            while (position_ < text_.size()
                   && std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
            {
                ++position_;
            }
        }

        if (position_ < text_.size() && text_[position_] == '.')
        {
            ++position_;
            const std::size_t fractionStart = position_;
            while (position_ < text_.size()
                   && std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
            {
                ++position_;
            }
            if (position_ == fractionStart)
            {
                error = errorAt("JSON fraction requires digits");
                return false;
            }
        }

        if (position_ < text_.size()
            && (text_[position_] == 'e' || text_[position_] == 'E'))
        {
            ++position_;
            if (position_ < text_.size()
                && (text_[position_] == '+' || text_[position_] == '-'))
            {
                ++position_;
            }
            const std::size_t exponentStart = position_;
            while (position_ < text_.size()
                   && std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
            {
                ++position_;
            }
            if (position_ == exponentStart)
            {
                error = errorAt("JSON exponent requires digits");
                return false;
            }
        }

        try
        {
            value = JsonValue {};
            value.type = JsonValue::Type::Number;
            value.numberValue = std::stod(text_.substr(start, position_ - start));
        }
        catch (...)
        {
            error = errorAt("could not parse JSON number");
            return false;
        }
        return std::isfinite(value.numberValue);
    }

    bool parseLiteral(const char* literal,
                      JsonValue::Type type,
                      JsonValue& value,
                      std::string& error,
                      bool booleanValue)
    {
        const std::string token(literal);
        if (text_.compare(position_, token.size(), token) != 0)
        {
            error = errorAt("invalid JSON literal");
            return false;
        }
        position_ += token.size();
        value = JsonValue {};
        value.type = type;
        value.booleanValue = booleanValue;
        return true;
    }

    bool consume(char expected)
    {
        if (position_ >= text_.size() || text_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    void skipWhitespace()
    {
        while (position_ < text_.size()
               && std::isspace(static_cast<unsigned char>(text_[position_])) != 0)
        {
            ++position_;
        }
    }

    std::string errorAt(const std::string& message) const
    {
        return message + " at byte " + std::to_string(position_);
    }

    std::string text_;
    std::size_t position_ = 0U;
};

const JsonValue* member(const JsonValue& object, const char* key)
{
    if (object.type != JsonValue::Type::Object)
        return nullptr;
    const auto found = object.objectValue.find(key);
    return found == object.objectValue.end() ? nullptr : &found->second;
}

bool requiredString(const JsonValue& object,
                    const char* key,
                    std::string& output,
                    std::string& error)
{
    const JsonValue* value = member(object, key);
    if (value == nullptr || value->type != JsonValue::Type::String || value->stringValue.empty())
    {
        error = std::string("pedal.json requires non-empty string '") + key + "'";
        return false;
    }
    output = value->stringValue;
    return true;
}

void optionalString(const JsonValue& object, const char* key, std::string& output)
{
    const JsonValue* value = member(object, key);
    if (value != nullptr && value->type == JsonValue::Type::String)
        output = value->stringValue;
}

void optionalNumber(const JsonValue& object, const char* key, double& output)
{
    const JsonValue* value = member(object, key);
    if (value != nullptr && value->type == JsonValue::Type::Number)
        output = value->numberValue;
}

bool validId(const std::string& id)
{
    if (id.empty())
        return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        const unsigned char uc = static_cast<unsigned char>(c);
        return std::islower(uc) != 0 || std::isdigit(uc) != 0 || c == '_';
    });
}

} // namespace

bool validatePedalPackageManifest(const PedalPackageManifest& manifest,
                                  std::string& error)
{
    if (!validId(manifest.id))
    {
        error = "pedal id must use lowercase snake_case characters only";
        return false;
    }
    if (manifest.displayName.empty())
    {
        error = "pedal displayName must not be empty";
        return false;
    }
    if (manifest.circuitFile.empty())
    {
        error = "pedal circuit.file must not be empty";
        return false;
    }
    if (!(manifest.faceplateAspectRatio > 0.1 && manifest.faceplateAspectRatio < 10.0))
    {
        error = "ui.faceplateAspectRatio must be between 0.1 and 10";
        return false;
    }

    std::set<std::string> ids;
    for (const auto& control : manifest.controls)
    {
        if (control.id.empty())
        {
            error = "every pedal control requires an id";
            return false;
        }
        if (!ids.insert(control.id).second)
        {
            error = "duplicate pedal control id: " + control.id;
            return false;
        }
        if (control.type != "knob" && control.type != "switch" && control.type != "toggle")
        {
            error = "unsupported pedal control type for " + control.id + ": " + control.type;
            return false;
        }
        if (control.x < 0.0 || control.x > 1.0 || control.y < 0.0 || control.y > 1.0)
        {
            error = "control coordinates must be normalized from 0.0 to 1.0: " + control.id;
            return false;
        }
        if (!(control.size > 0.0 && control.size <= 4.0))
        {
            error = "control size must be greater than 0 and at most 4: " + control.id;
            return false;
        }
    }

    error.clear();
    return true;
}

bool loadPedalPackageManifest(const std::string& manifestPath,
                              PedalPackageManifest& manifest,
                              std::string& error)
{
    std::ifstream input(manifestPath, std::ios::binary);
    if (!input)
    {
        error = "could not open pedal package manifest: " + manifestPath;
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    JsonValue root;
    JsonParser parser(buffer.str());
    if (!parser.parse(root, error))
    {
        error = "invalid pedal package JSON: " + error;
        return false;
    }
    if (root.type != JsonValue::Type::Object)
    {
        error = "pedal.json root must be a JSON object";
        return false;
    }

    PedalPackageManifest loaded;
    loaded.manifestPath = manifestPath;
    loaded.packageDirectory = std::filesystem::path(manifestPath).parent_path().string();

    if (!requiredString(root, "id", loaded.id, error)
        || !requiredString(root, "displayName", loaded.displayName, error))
    {
        return false;
    }
    optionalString(root, "modelStatus", loaded.modelStatus);
    optionalString(root, "category", loaded.category);
    optionalString(root, "inspiredBy", loaded.inspiredBy);
    optionalString(root, "description", loaded.description);

    const JsonValue* assets = member(root, "assets");
    if (assets != nullptr)
    {
        if (assets->type != JsonValue::Type::Object)
        {
            error = "pedal assets must be a JSON object";
            return false;
        }
        optionalString(*assets, "faceplate", loaded.assets.faceplate);
        optionalString(*assets, "thumbnail", loaded.assets.thumbnail);
        optionalString(*assets, "icon", loaded.assets.icon);
    }

    const JsonValue* circuit = member(root, "circuit");
    if (circuit == nullptr || circuit->type != JsonValue::Type::Object
        || !requiredString(*circuit, "file", loaded.circuitFile, error))
    {
        if (error.empty())
            error = "pedal.json requires circuit.file";
        return false;
    }

    const JsonValue* ui = member(root, "ui");
    if (ui != nullptr)
    {
        if (ui->type != JsonValue::Type::Object)
        {
            error = "pedal ui must be a JSON object";
            return false;
        }
        optionalNumber(*ui, "faceplateAspectRatio", loaded.faceplateAspectRatio);
        optionalString(*ui, "controlStyle", loaded.controlStyle);
    }

    const JsonValue* controls = member(root, "controls");
    if (controls != nullptr)
    {
        if (controls->type != JsonValue::Type::Array)
        {
            error = "pedal controls must be a JSON array";
            return false;
        }
        for (const JsonValue& item : controls->arrayValue)
        {
            if (item.type != JsonValue::Type::Object)
            {
                error = "every pedal control must be a JSON object";
                return false;
            }
            PedalPackageControl control;
            if (!requiredString(item, "id", control.id, error))
                return false;
            control.label = control.id;
            optionalString(item, "label", control.label);
            control.type = "knob";
            optionalString(item, "type", control.type);
            optionalNumber(item, "x", control.x);
            optionalNumber(item, "y", control.y);
            optionalNumber(item, "size", control.size);
            loaded.controls.push_back(std::move(control));
        }
    }

    const JsonValue* presets = member(root, "presets");
    if (presets != nullptr)
    {
        if (presets->type != JsonValue::Type::Array)
        {
            error = "pedal presets must be a JSON array";
            return false;
        }
        for (const JsonValue& item : presets->arrayValue)
        {
            if (item.type != JsonValue::Type::String || item.stringValue.empty())
            {
                error = "pedal preset entries must be non-empty strings";
                return false;
            }
            loaded.presets.push_back(item.stringValue);
        }
    }

    if (!validatePedalPackageManifest(loaded, error))
        return false;

    manifest = std::move(loaded);
    return true;
}

std::string resolvePedalPackagePath(const PedalPackageManifest& manifest,
                                    const std::string& relativePath)
{
    if (relativePath.empty())
        return {};
    const std::filesystem::path path(relativePath);
    if (path.is_absolute())
        return path.lexically_normal().string();
    return (std::filesystem::path(manifest.packageDirectory) / path)
        .lexically_normal()
        .string();
}

} // namespace circuitpedal
