#pragma once

#include <string>
#include <utility>
#include <vector>

namespace json {

enum class Type
{
	Null,
	Bool,
	Number,
	String,
	Array,
	Object
};

class Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

class Value
{
public:
	Value() : type_(Type::Null) {}
	Value(bool value) : type_(Type::Bool), bool_(value) {}
	Value(int value) : type_(Type::Number), num_(static_cast<double>(value)) {}
	Value(double value) : type_(Type::Number), num_(value) {}
	Value(const char* value) : type_(Type::String), str_(value) {}
	Value(std::string value) : type_(Type::String), str_(std::move(value)) {}

	static Value MakeArray() { Value v; v.type_ = Type::Array; return v; }
	static Value MakeObject() { Value v; v.type_ = Type::Object; return v; }

	Type type() const { return type_; }
	bool is_null() const { return type_ == Type::Null; }

	bool as_bool(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
	double as_double(double fallback = 0.0) const { return type_ == Type::Number ? num_ : fallback; }
	int as_int(int fallback = 0) const { return type_ == Type::Number ? static_cast<int>(num_) : fallback; }
	float as_float(float fallback = 0.0f) const { return type_ == Type::Number ? static_cast<float>(num_) : fallback; }
	const std::string& as_string(const std::string& fallback = empty_string()) const
	{
		return type_ == Type::String ? str_ : fallback;
	}

	const Array& as_array() const { return arr_; }
	const Object& as_object() const { return obj_; }

	Array& array()
	{
		if (type_ != Type::Array) { type_ = Type::Array; arr_.clear(); }
		return arr_;
	}

	Object& object()
	{
		if (type_ != Type::Object) { type_ = Type::Object; obj_.clear(); }
		return obj_;
	}

	bool has(const std::string& key) const;
	const Value& get(const std::string& key) const;
	void set(const std::string& key, Value value);
	void push_back(Value value) { array().push_back(std::move(value)); }

private:
	static const std::string& empty_string()
	{
		static const std::string value;
		return value;
	}

	Type type_;
	bool bool_ = false;
	double num_ = 0.0;
	std::string str_;
	Array arr_;
	Object obj_;
};

std::string stringify(const Value& value, int indent = 2);
bool parse(const std::string& text, Value& out, std::string* error = nullptr);
bool load_file(const std::string& path, Value& out, std::string* error = nullptr);
bool save_file(const std::string& path, const Value& value, std::string* error = nullptr);

} // namespace json
