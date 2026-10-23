#include "json.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace json {

bool Value::has(const std::string& key) const
{
	if (type_ != Type::Object)
		return false;
	for (const auto& entry : obj_)
		if (entry.first == key)
			return true;
	return false;
}

const Value& Value::get(const std::string& key) const
{
	static const Value null_value;
	if (type_ != Type::Object)
		return null_value;
	for (const auto& entry : obj_)
		if (entry.first == key)
			return entry.second;
	return null_value;
}

void Value::set(const std::string& key, Value value)
{
	if (type_ != Type::Object) { type_ = Type::Object; obj_.clear(); }
	for (auto& entry : obj_)
	{
		if (entry.first == key)
		{
			entry.second = std::move(value);
			return;
		}
	}
	obj_.emplace_back(key, std::move(value));
}

namespace {

void escape_into(const std::string& value, std::string& out)
{
	out.push_back('"');
	for (unsigned char c : value)
	{
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buffer[8];
				std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
				out += buffer;
			}
			else
			{
				out.push_back(static_cast<char>(c));
			}
			break;
		}
	}
	out.push_back('"');
}

void format_number(double value, std::string& out)
{
	if (std::isfinite(value) && value == std::floor(value) && std::fabs(value) < 1e15)
	{
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
		out += buffer;
		return;
	}
	char buffer[48];
	std::snprintf(buffer, sizeof(buffer), "%.9g", value);
	out += buffer;
}

void write_value(const Value& value, int indent, int depth, std::string& out)
{
	const std::string pad(static_cast<size_t>(indent) * static_cast<size_t>(depth), ' ');
	const std::string pad_inner(static_cast<size_t>(indent) * static_cast<size_t>(depth + 1), ' ');

	switch (value.type())
	{
	case Type::Null:
		out += "null";
		break;
	case Type::Bool:
		out += value.as_bool() ? "true" : "false";
		break;
	case Type::Number:
		format_number(value.as_double(), out);
		break;
	case Type::String:
		escape_into(value.as_string(), out);
		break;
	case Type::Array:
	{
		const Array& items = value.as_array();
		if (items.empty()) { out += "[]"; break; }
		out += "[\n";
		for (size_t i = 0; i < items.size(); ++i)
		{
			out += pad_inner;
			write_value(items[i], indent, depth + 1, out);
			if (i + 1 < items.size()) out += ",";
			out += "\n";
		}
		out += pad;
		out += "]";
		break;
	}
	case Type::Object:
	{
		const Object& fields = value.as_object();
		if (fields.empty()) { out += "{}"; break; }
		out += "{\n";
		for (size_t i = 0; i < fields.size(); ++i)
		{
			out += pad_inner;
			escape_into(fields[i].first, out);
			out += ": ";
			write_value(fields[i].second, indent, depth + 1, out);
			if (i + 1 < fields.size()) out += ",";
			out += "\n";
		}
		out += pad;
		out += "}";
		break;
	}
	}
}

class Parser
{
public:
	explicit Parser(const std::string& text) : text_(text) {}

	bool parse_value(Value& out)
	{
		skip_ws();
		if (pos_ >= text_.size()) return fail("unexpected end of input");

		const char c = text_[pos_];
		if (c == '{') return parse_object(out);
		if (c == '[') return parse_array(out);
		if (c == '"') { std::string s; if (!parse_string(s)) return false; out = Value(s); return true; }
		if (c == 't' || c == 'f') return parse_bool(out);
		if (c == 'n') return parse_null(out);
		if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
		return fail("unexpected character");
	}

	const std::string& error() const { return error_; }

private:
	const std::string& text_;
	size_t pos_ = 0;
	std::string error_;

	bool fail(const char* message)
	{
		if (error_.empty())
			error_ = message;
		return false;
	}

	void skip_ws()
	{
		while (pos_ < text_.size())
		{
			const char c = text_[pos_];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos_; continue; }
			break;
		}
	}

	bool expect(char c)
	{
		if (pos_ >= text_.size() || text_[pos_] != c)
			return fail("expected a different character");
		++pos_;
		return true;
	}

	bool parse_object(Value& out)
	{
		out = Value::MakeObject();
		if (!expect('{')) return false;
		skip_ws();
		if (pos_ < text_.size() && text_[pos_] == '}') { ++pos_; return true; }

		while (true)
		{
			skip_ws();
			std::string key;
			if (pos_ >= text_.size() || text_[pos_] != '"') return fail("expected a key string");
			if (!parse_string(key)) return false;
			skip_ws();
			if (!expect(':')) return false;
			Value value;
			if (!parse_value(value)) return false;
			out.set(key, std::move(value));
			skip_ws();
			if (pos_ >= text_.size()) return fail("unterminated object");
			if (text_[pos_] == ',') { ++pos_; continue; }
			if (text_[pos_] == '}') { ++pos_; return true; }
			return fail("expected ',' or '}'");
		}
	}

	bool parse_array(Value& out)
	{
		out = Value::MakeArray();
		if (!expect('[')) return false;
		skip_ws();
		if (pos_ < text_.size() && text_[pos_] == ']') { ++pos_; return true; }

		while (true)
		{
			Value value;
			if (!parse_value(value)) return false;
			out.push_back(std::move(value));
			skip_ws();
			if (pos_ >= text_.size()) return fail("unterminated array");
			if (text_[pos_] == ',') { ++pos_; continue; }
			if (text_[pos_] == ']') { ++pos_; return true; }
			return fail("expected ',' or ']'");
		}
	}

	static void append_utf8(unsigned int code_point, std::string& out)
	{
		if (code_point <= 0x7F)
		{
			out.push_back(static_cast<char>(code_point));
		}
		else if (code_point <= 0x7FF)
		{
			out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
			out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
		}
		else
		{
			out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
			out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
		}
	}

	bool parse_string(std::string& out)
	{
		if (!expect('"')) return false;
		out.clear();

		while (true)
		{
			if (pos_ >= text_.size()) return fail("unterminated string");
			const char c = text_[pos_++];
			if (c == '"') return true;

			if (c != '\\')
			{
				out.push_back(c);
				continue;
			}

			if (pos_ >= text_.size()) return fail("unterminated escape");
			const char esc = text_[pos_++];
			switch (esc)
			{
			case '"': out.push_back('"'); break;
			case '\\': out.push_back('\\'); break;
			case '/': out.push_back('/'); break;
			case 'b': out.push_back('\b'); break;
			case 'f': out.push_back('\f'); break;
			case 'n': out.push_back('\n'); break;
			case 'r': out.push_back('\r'); break;
			case 't': out.push_back('\t'); break;
			case 'u':
			{
				if (pos_ + 4 > text_.size()) return fail("truncated unicode escape");
				unsigned int code_point = 0;
				for (int i = 0; i < 4; ++i)
				{
					const char hex = text_[pos_++];
					code_point <<= 4;
					if (hex >= '0' && hex <= '9') code_point |= static_cast<unsigned int>(hex - '0');
					else if (hex >= 'a' && hex <= 'f') code_point |= static_cast<unsigned int>(hex - 'a' + 10);
					else if (hex >= 'A' && hex <= 'F') code_point |= static_cast<unsigned int>(hex - 'A' + 10);
					else return fail("invalid unicode escape");
				}
				append_utf8(code_point, out);
				break;
			}
			default:
				return fail("invalid escape sequence");
			}
		}
	}

	bool parse_bool(Value& out)
	{
		if (text_.compare(pos_, 4, "true") == 0) { pos_ += 4; out = Value(true); return true; }
		if (text_.compare(pos_, 5, "false") == 0) { pos_ += 5; out = Value(false); return true; }
		return fail("invalid literal");
	}

	bool parse_null(Value& out)
	{
		if (text_.compare(pos_, 4, "null") == 0) { pos_ += 4; out = Value(); return true; }
		return fail("invalid literal");
	}

	bool parse_number(Value& out)
	{
		const size_t start = pos_;
		if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
		while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
		if (pos_ < text_.size() && text_[pos_] == '.')
		{
			++pos_;
			while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
		}
		if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
		{
			++pos_;
			if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
			while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
		}

		if (pos_ == start) return fail("invalid number");
		out = Value(std::strtod(text_.c_str() + start, nullptr));
		return true;
	}
};

} // namespace

std::string stringify(const Value& value, int indent)
{
	std::string out;
	write_value(value, indent, 0, out);
	return out;
}

bool parse(const std::string& text, Value& out, std::string* error)
{
	Parser parser(text);
	if (!parser.parse_value(out))
	{
		if (error) *error = parser.error();
		return false;
	}
	return true;
}

bool load_file(const std::string& path, Value& out, std::string* error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		if (error) *error = "could not open file";
		return false;
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();
	return parse(buffer.str(), out, error);
}

bool save_file(const std::string& path, const Value& value, std::string* error)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		if (error) *error = "could not open file for writing";
		return false;
	}

	const std::string text = stringify(value);
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return file.good();
}

} // namespace json
