// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Session.cpp
 *
 * @brief Implementation of the Merge20 work-session save/restore
 * (S8, design section 3.6, spec sections 6 and 6.1).
 *
 * The JSON reader/writer here is deliberately minimal: the session
 * structure is fixed, so the parser only needs objects, arrays,
 * strings, integers, booleans and null, and the writer only emits
 * those. Numbers are integers only (sizes, times, counts, enum
 * ordinals); a non-integer number in an input file is a parse error.
 * Strings are stored as wide text in the DOM and converted to/from
 * UTF-8 at the byte boundary, so the file is plain UTF-8 JSON.
 *
 * This file stays ASCII: no Japanese literal is needed here (all
 * user-visible words are composed by the MFC adapter through `_()`).
 */
#include "pch.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include "Merge20/Merge20Session.h"

namespace
{

	// Decode one wide unit sequence starting at i into a code point
	// (combining a surrogate pair when wchar_t is 16-bit); advances i.
	// A lone surrogate becomes U+FFFD.
	char32_t NextCodePoint(const String& text, size_t& i)
	{
		wchar_t w = text[i++];
		if (sizeof(wchar_t) == 2 && w >= 0xD800 && w <= 0xDBFF && i < text.size())
		{
			wchar_t w2 = text[i];
			if (w2 >= 0xDC00 && w2 <= 0xDFFF)
			{
				++i;
				return 0x10000 + ((static_cast<char32_t>(w - 0xD800) << 10) |
					static_cast<char32_t>(w2 - 0xDC00));
			}
		}
		if (sizeof(wchar_t) == 2 && w >= 0xD800 && w <= 0xDFFF)
			return 0xFFFD;
		return static_cast<char32_t>(w);
	}

	// Append one code point as wide text (as a surrogate pair when
	// wchar_t is 16-bit and the point is astral). Out-of-range points
	// and surrogate halves become U+FFFD.
	void AppendCodePoint(String& out, char32_t cp)
	{
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			cp = 0xFFFD;
		if (sizeof(wchar_t) == 2 && cp > 0xFFFF)
		{
			cp -= 0x10000;
			out += static_cast<wchar_t>(0xD800 + (cp >> 10));
			out += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
		}
		else
		{
			out += static_cast<wchar_t>(cp);
		}
	}

	// Append one code point as UTF-8 bytes.
	void AppendUtf8(std::string& out, char32_t cp)
	{
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			cp = 0xFFFD;
		if (cp < 0x80)
		{
			out += static_cast<char>(cp);
		}
		else if (cp < 0x800)
		{
			out += static_cast<char>(0xC0 | (cp >> 6));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000)
		{
			out += static_cast<char>(0xE0 | (cp >> 12));
			out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else
		{
			out += static_cast<char>(0xF0 | (cp >> 18));
			out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
	}

	// Decode one UTF-8 sequence starting at i into a code point;
	// advances i. An invalid sequence becomes U+FFFD and consumes one
	// byte, so decoding never fails.
	char32_t NextUtf8CodePoint(const std::string& s, size_t& i)
	{
		const auto byteAt = [&s](size_t at) -> unsigned char
		{
			return static_cast<unsigned char>(s[at]);
		};
		unsigned char b0 = byteAt(i);
		if (b0 < 0x80)
		{
			++i;
			return b0;
		}
		const auto isCont = [&byteAt](unsigned char b) { return (b & 0xC0) == 0x80; };
		if ((b0 & 0xE0) == 0xC0 && i + 1 < s.size() && isCont(byteAt(i + 1)))
		{
			char32_t cp = ((b0 & 0x1F) << 6) | (byteAt(i + 1) & 0x3F);
			i += 2;
			return cp < 0x80 ? 0xFFFD : cp; // overlong form
		}
		if ((b0 & 0xF0) == 0xE0 && i + 2 < s.size() && isCont(byteAt(i + 1)) && isCont(byteAt(i + 2)))
		{
			char32_t cp = ((b0 & 0x0F) << 12) | ((byteAt(i + 1) & 0x3F) << 6) | (byteAt(i + 2) & 0x3F);
			i += 3;
			return (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) ? 0xFFFD : cp;
		}
		if ((b0 & 0xF8) == 0xF0 && i + 3 < s.size() && isCont(byteAt(i + 1)) && isCont(byteAt(i + 2)) && isCont(byteAt(i + 3)))
		{
			char32_t cp = ((b0 & 0x07) << 18) | ((byteAt(i + 1) & 0x3F) << 12) |
				((byteAt(i + 2) & 0x3F) << 6) | (byteAt(i + 3) & 0x3F);
			i += 4;
			return (cp < 0x10000 || cp > 0x10FFFF) ? 0xFFFD : cp;
		}
		++i;
		return 0xFFFD;
	}

	// Maximum JSON nesting depth (the session format nests at most
	// 4 levels; the cap only guards against hostile input).
	constexpr int kMaxJsonDepth = 32;

	// Minimal recursive-descent JSON parser over UTF-8 bytes.
	class JsonParser
	{
	public:
		JsonParser(const std::string& text, std::string& errDetail)
			: m_text(text), m_err(errDetail)
		{
		}

		bool Parse(Merge20JsonValue& out)
		{
			m_pos = 0;
			// A leading UTF-8 BOM is tolerated (the writer never emits one).
			if (m_text.size() >= 3 &&
				static_cast<unsigned char>(m_text[0]) == 0xEF &&
				static_cast<unsigned char>(m_text[1]) == 0xBB &&
				static_cast<unsigned char>(m_text[2]) == 0xBF)
				m_pos = 3;
			if (!ParseValue(out, 0))
				return false;
			SkipWs();
			if (m_pos != m_text.size())
				return Fail(m_pos, "unexpected trailing characters");
			return true;
		}

	private:
		const std::string& m_text;
		std::string& m_err;
		size_t m_pos = 0;

		bool Fail(size_t offset, const char* message)
		{
			m_err = "offset " + std::to_string(offset) + ": " + message;
			return false;
		}

		void SkipWs()
		{
			while (m_pos < m_text.size())
			{
				char c = m_text[m_pos];
				if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
					break;
				++m_pos;
			}
		}

		bool Consume(char c)
		{
			if (m_pos < m_text.size() && m_text[m_pos] == c)
			{
				++m_pos;
				return true;
			}
			return false;
		}

		bool ParseValue(Merge20JsonValue& out, int depth)
		{
			if (depth > kMaxJsonDepth)
				return Fail(m_pos, "nesting too deep");
			SkipWs();
			if (m_pos >= m_text.size())
				return Fail(m_pos, "unexpected end of input");
			char c = m_text[m_pos];
			switch (c)
			{
			case '{':
				return ParseObject(out, depth);
			case '[':
				return ParseArray(out, depth);
			case '"':
				{
					String text;
					if (!ParseString(text))
						return false;
					out = Merge20JsonText(text);
					return true;
				}
			case 't':
				if (m_text.compare(m_pos, 4, "true") == 0)
				{
					m_pos += 4;
					out = Merge20JsonBool(true);
					return true;
				}
				return Fail(m_pos, "invalid literal");
			case 'f':
				if (m_text.compare(m_pos, 5, "false") == 0)
				{
					m_pos += 5;
					out = Merge20JsonBool(false);
					return true;
				}
				return Fail(m_pos, "invalid literal");
			case 'n':
				if (m_text.compare(m_pos, 4, "null") == 0)
				{
					m_pos += 4;
					out = Merge20JsonNull();
					return true;
				}
				return Fail(m_pos, "invalid literal");
			default:
				if (c == '-' || (c >= '0' && c <= '9'))
					return ParseNumber(out);
				return Fail(m_pos, "unexpected character");
			}
		}

		bool ParseObject(Merge20JsonValue& out, int depth)
		{
			++m_pos; // '{'
			std::vector<std::pair<String, Merge20JsonValue>> members;
			SkipWs();
			if (Consume('}'))
			{
				out = Merge20JsonObject(std::move(members));
				return true;
			}
			for (;;)
			{
				SkipWs();
				if (m_pos >= m_text.size() || m_text[m_pos] != '"')
					return Fail(m_pos, "expected object key");
				String key;
				if (!ParseString(key))
					return false;
				SkipWs();
				if (!Consume(':'))
					return Fail(m_pos, "expected ':' in object");
				Merge20JsonValue value;
				if (!ParseValue(value, depth + 1))
					return false;
				members.emplace_back(std::move(key), std::move(value));
				SkipWs();
				if (Consume('}'))
					break;
				if (!Consume(','))
					return Fail(m_pos, "expected ',' or '}' in object");
			}
			out = Merge20JsonObject(std::move(members));
			return true;
		}

		bool ParseArray(Merge20JsonValue& out, int depth)
		{
			++m_pos; // '['
			std::vector<Merge20JsonValue> values;
			SkipWs();
			if (Consume(']'))
			{
				out = Merge20JsonArray(std::move(values));
				return true;
			}
			for (;;)
			{
				Merge20JsonValue value;
				if (!ParseValue(value, depth + 1))
					return false;
				values.push_back(std::move(value));
				SkipWs();
				if (Consume(']'))
					break;
				if (!Consume(','))
					return Fail(m_pos, "expected ',' or ']' in array");
			}
			out = Merge20JsonArray(std::move(values));
			return true;
		}

		bool ParseHex4(uint32_t& value)
		{
			if (m_pos + 4 > m_text.size())
				return Fail(m_pos, "truncated \\u escape");
			value = 0;
			for (int k = 0; k < 4; ++k)
			{
				char c = m_text[m_pos++];
				value <<= 4;
				if (c >= '0' && c <= '9')
					value |= static_cast<uint32_t>(c - '0');
				else if (c >= 'a' && c <= 'f')
					value |= static_cast<uint32_t>(c - 'a' + 10);
				else if (c >= 'A' && c <= 'F')
					value |= static_cast<uint32_t>(c - 'A' + 10);
				else
					return Fail(m_pos, "invalid \\u escape");
			}
			return true;
		}

		bool ParseString(String& out)
		{
			++m_pos; // opening '"'
			out.clear();
			while (m_pos < m_text.size())
			{
				unsigned char c = static_cast<unsigned char>(m_text[m_pos]);
				if (c == '"')
				{
					++m_pos;
					return true;
				}
				if (c < 0x20)
					return Fail(m_pos, "unescaped control character in string");
				if (c == '\\')
				{
					++m_pos;
					if (m_pos >= m_text.size())
						return Fail(m_pos, "truncated escape");
					char e = m_text[m_pos++];
					switch (e)
					{
					case '"': out += L'"'; break;
					case '\\': out += L'\\'; break;
					case '/': out += L'/'; break;
					case 'b': out += L'\b'; break;
					case 'f': out += L'\f'; break;
					case 'n': out += L'\n'; break;
					case 'r': out += L'\r'; break;
					case 't': out += L'\t'; break;
					case 'u':
						{
							uint32_t cp = 0;
							if (!ParseHex4(cp))
								return false;
							if (cp >= 0xD800 && cp <= 0xDBFF &&
								m_pos + 1 < m_text.size() && m_text[m_pos] == '\\' && m_text[m_pos + 1] == 'u')
							{
								m_pos += 2;
								uint32_t low = 0;
								if (!ParseHex4(low))
									return false;
								if (low >= 0xDC00 && low <= 0xDFFF)
									cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
								else
									cp = 0xFFFD;
							}
							else if (cp >= 0xD800 && cp <= 0xDFFF)
							{
								cp = 0xFFFD;
							}
							AppendCodePoint(out, cp);
						}
						break;
					default:
						return Fail(m_pos, "invalid escape");
					}
					continue;
				}
				// Raw UTF-8 bytes (a lone byte is replaced, not an error).
				size_t at = m_pos;
				AppendCodePoint(out, NextUtf8CodePoint(m_text, at));
				m_pos = at;
			}
			return Fail(m_pos, "unterminated string");
		}

		bool ParseNumber(Merge20JsonValue& out)
		{
			const size_t start = m_pos;
			bool negative = false;
			if (m_text[m_pos] == '-')
			{
				negative = true;
				++m_pos;
			}
			if (m_pos >= m_text.size() || !std::isdigit(static_cast<unsigned char>(m_text[m_pos])))
				return Fail(start, "invalid number");
			uint64_t magnitude = 0;
			while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos])))
			{
				const unsigned digit = static_cast<unsigned>(m_text[m_pos] - '0');
				if (magnitude > (UINT64_MAX - digit) / 10)
					return Fail(start, "number out of range");
				magnitude = magnitude * 10 + digit;
				++m_pos;
			}
			// This format uses integers only; a fraction or exponent is
			// not a valid session number.
			if (m_pos < m_text.size() && (m_text[m_pos] == '.' || m_text[m_pos] == 'e' || m_text[m_pos] == 'E'))
				return Fail(m_pos, "non-integer number");
			int64_t value = 0;
			if (negative)
			{
				if (magnitude > static_cast<uint64_t>(INT64_MAX) + 1)
					return Fail(start, "number out of range");
				value = (magnitude == static_cast<uint64_t>(INT64_MAX) + 1)
					? INT64_MIN : -static_cast<int64_t>(magnitude);
			}
			else
			{
				if (magnitude > static_cast<uint64_t>(INT64_MAX))
					return Fail(start, "number out of range");
				value = static_cast<int64_t>(magnitude);
			}
			out = Merge20JsonInt(value);
			return true;
		}
	};

	// Write one JSON string (quoted, escaped, UTF-8).
	void WriteJsonString(const String& text, std::string& out)
	{
		out += '"';
		size_t i = 0;
		while (i < text.size())
		{
			char32_t cp = NextCodePoint(text, i);
			switch (cp)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (cp < 0x20)
				{
					const char* hex = "0123456789abcdef";
					out += "\\u00";
					out += hex[(cp >> 4) & 0xF];
					out += hex[cp & 0xF];
				}
				else
				{
					AppendUtf8(out, cp);
				}
				break;
			}
		}
		out += '"';
	}

	// Write one JSON value, pretty-printed with a 2-space indent.
	void WriteJsonValue(const Merge20JsonValue& value, std::string& out, int indent)
	{
		const std::string pad(static_cast<size_t>(indent) * 2, ' ');
		const std::string padInner(static_cast<size_t>(indent + 1) * 2, ' ');
		switch (value.type)
		{
		case Merge20JsonValue::Type::Null:
			out += "null";
			break;
		case Merge20JsonValue::Type::Bool:
			out += value.boolean ? "true" : "false";
			break;
		case Merge20JsonValue::Type::Int:
			out += std::to_string(value.integer);
			break;
		case Merge20JsonValue::Type::Text:
			WriteJsonString(value.text, out);
			break;
		case Merge20JsonValue::Type::Array:
			if (value.array.empty())
			{
				out += "[]";
				break;
			}
			out += "[\n";
			for (size_t i = 0; i < value.array.size(); ++i)
			{
				out += padInner;
				WriteJsonValue(value.array[i], out, indent + 1);
				if (i + 1 < value.array.size())
					out += ',';
				out += '\n';
			}
			out += pad;
			out += ']';
			break;
		case Merge20JsonValue::Type::Object:
			if (value.object.empty())
			{
				out += "{}";
				break;
			}
			out += "{\n";
			for (size_t i = 0; i < value.object.size(); ++i)
			{
				out += padInner;
				WriteJsonString(value.object[i].first, out);
				out += ": ";
				WriteJsonValue(value.object[i].second, out, indent + 1);
				if (i + 1 < value.object.size())
					out += ',';
				out += '\n';
			}
			out += pad;
			out += '}';
			break;
		}
	}

} // namespace

std::string Merge20Utf8Encode(const String& text)
{
	std::string out;
	size_t i = 0;
	while (i < text.size())
		AppendUtf8(out, NextCodePoint(text, i));
	return out;
}

bool Merge20Utf8Decode(const std::string& utf8, String& out)
{
	out.clear();
	size_t i = 0;
	while (i < utf8.size())
		AppendCodePoint(out, NextUtf8CodePoint(utf8, i));
	return true;
}

const Merge20JsonValue* Merge20JsonValue::Find(const String& key) const
{
	if (type != Type::Object)
		return nullptr;
	for (const auto& member : object)
	{
		if (member.first == key)
			return &member.second;
	}
	return nullptr;
}

bool Merge20JsonValue::AsBool(bool defaultValue) const
{
	return type == Type::Bool ? boolean : defaultValue;
}

int64_t Merge20JsonValue::AsInt(int64_t defaultValue) const
{
	return type == Type::Int ? integer : defaultValue;
}

String Merge20JsonValue::AsText(const String& defaultValue) const
{
	return type == Type::Text ? text : defaultValue;
}

Merge20JsonValue Merge20JsonNull()
{
	return Merge20JsonValue{};
}

Merge20JsonValue Merge20JsonBool(bool value)
{
	Merge20JsonValue v;
	v.type = Merge20JsonValue::Type::Bool;
	v.boolean = value;
	return v;
}

Merge20JsonValue Merge20JsonInt(int64_t value)
{
	Merge20JsonValue v;
	v.type = Merge20JsonValue::Type::Int;
	v.integer = value;
	return v;
}

Merge20JsonValue Merge20JsonText(const String& value)
{
	Merge20JsonValue v;
	v.type = Merge20JsonValue::Type::Text;
	v.text = value;
	return v;
}

Merge20JsonValue Merge20JsonArray(std::vector<Merge20JsonValue> values)
{
	Merge20JsonValue v;
	v.type = Merge20JsonValue::Type::Array;
	v.array = std::move(values);
	return v;
}

Merge20JsonValue Merge20JsonObject(std::vector<std::pair<String, Merge20JsonValue>> members)
{
	Merge20JsonValue v;
	v.type = Merge20JsonValue::Type::Object;
	v.object = std::move(members);
	return v;
}

bool Merge20JsonParse(const std::string& text, Merge20JsonValue& out, std::string& errDetail)
{
	JsonParser parser(text, errDetail);
	errDetail.clear();
	return parser.Parse(out);
}

std::string Merge20JsonStringify(const Merge20JsonValue& value)
{
	std::string out;
	WriteJsonValue(value, out, 0);
	return out;
}

bool Merge20SessionFileChanged(const Merge20SessionFile& saved, const Merge20FileIdentity& current)
{
	if (saved.leftSize != current.leftSize)
		return true;
	if (saved.leftMtime != current.leftMtime)
		return true;
	if (saved.rightSize != current.rightSize)
		return true;
	if (saved.rightMtime != current.rightMtime)
		return true;
	if (saved.thirdSize != current.thirdSize)
		return true;
	if (saved.thirdMtime != current.thirdMtime)
		return true;
	if (saved.nsdiffs >= 0 && current.nsdiffs >= 0 && saved.nsdiffs != current.nsdiffs)
		return true;
	if (saved.nidiffs >= 0 && current.nidiffs >= 0 && saved.nidiffs != current.nidiffs)
		return true;
	return false;
}

namespace
{

	// Field readers for the session format. A field that is present
	// but has the wrong type (or an out-of-range integer) fails the
	// entry it belongs to; a missing field keeps the caller's default.
	bool GetBoolField(const Merge20JsonValue& obj, const String& key, bool& out)
	{
		const Merge20JsonValue* value = obj.Find(key);
		if (value == nullptr)
			return true;
		if (value->type != Merge20JsonValue::Type::Bool)
			return false;
		out = value->boolean;
		return true;
	}

	bool GetIntField(const Merge20JsonValue& obj, const String& key, int64_t& out, int64_t minValue, int64_t maxValue)
	{
		const Merge20JsonValue* value = obj.Find(key);
		if (value == nullptr)
			return true;
		if (value->type != Merge20JsonValue::Type::Int ||
			value->integer < minValue || value->integer > maxValue)
			return false;
		out = value->integer;
		return true;
	}

	bool GetTextField(const Merge20JsonValue& obj, const String& key, String& out)
	{
		const Merge20JsonValue* value = obj.Find(key);
		if (value == nullptr)
			return true;
		if (value->type != Merge20JsonValue::Type::Text)
			return false;
		out = value->text;
		return true;
	}

	// Read one diff judgment. Returns false when the entry is unusable
	// (wrong type or out-of-range value); the caller skips that diff.
	bool ParseDiffJudgment(const Merge20JsonValue& value, Merge20DiffJudgment& out)
	{
		if (value.type != Merge20JsonValue::Type::Object)
			return false;
		int64_t diffIdx = -1, op = 0, state = 0;
		if (!GetIntField(value, L"diffIdx", diffIdx, 0, INT32_MAX))
			return false;
		if (diffIdx < 0)
			return false;
		if (!GetIntField(value, L"op", op, 0, 5))
			return false;
		if (!GetIntField(value, L"state", state, 0, 5))
			return false;
		out.diffIdx = static_cast<int>(diffIdx);
		out.op = static_cast<int>(op);
		out.state = static_cast<int>(state);
		const Merge20JsonValue* begin = value.Find(L"begin");
		const Merge20JsonValue* end = value.Find(L"end");
		if (begin != nullptr)
		{
			if (begin->type != Merge20JsonValue::Type::Array || begin->array.size() != 3)
				return false;
			for (int i = 0; i < 3; ++i)
			{
				if (begin->array[i].type != Merge20JsonValue::Type::Int ||
					begin->array[i].integer < -1 || begin->array[i].integer > INT32_MAX)
					return false;
				out.begin[i] = static_cast<int>(begin->array[i].integer);
			}
		}
		if (end != nullptr)
		{
			if (end->type != Merge20JsonValue::Type::Array || end->array.size() != 3)
				return false;
			for (int i = 0; i < 3; ++i)
			{
				if (end->array[i].type != Merge20JsonValue::Type::Int ||
					end->array[i].integer < -1 || end->array[i].integer > INT32_MAX)
					return false;
				out.end[i] = static_cast<int>(end->array[i].integer);
			}
		}
		const Merge20JsonValue* srcPanes = value.Find(L"srcPanes");
		if (srcPanes != nullptr)
		{
			if (srcPanes->type != Merge20JsonValue::Type::Array)
				return false;
			out.srcPanes.clear();
			for (const auto& pane : srcPanes->array)
			{
				if (pane.type != Merge20JsonValue::Type::Int ||
					pane.integer < 0 || pane.integer > 2)
					return false;
				out.srcPanes.push_back(static_cast<int>(pane.integer));
			}
		}
		return true;
	}

	// Read one split diff group. Returns false when the entry is
	// unusable (wrong type or out-of-range value); the caller skips
	// that group.
	bool ParseSplitGroup(const Merge20JsonValue& value, Merge20SplitGroup& out)
	{
		if (value.type != Merge20JsonValue::Type::Object)
			return false;
		int64_t op = 0, dbegin = 0, dend = 0, rows = 0;
		if (!GetIntField(value, L"op", op, 0, 5))
			return false;
		if (!GetIntField(value, L"dbegin", dbegin, 0, INT32_MAX))
			return false;
		if (!GetIntField(value, L"dend", dend, 0, INT32_MAX))
			return false;
		if (!GetIntField(value, L"rows", rows, 2, INT32_MAX))
			return false;
		out.op = static_cast<int>(op);
		out.dbegin = static_cast<int>(dbegin);
		out.dend = static_cast<int>(dend);
		out.rows = static_cast<int>(rows);
		const Merge20JsonValue* begin = value.Find(L"begin");
		const Merge20JsonValue* end = value.Find(L"end");
		const Merge20JsonValue* blank = value.Find(L"blank");
		if (begin == nullptr || end == nullptr || blank == nullptr)
			return false;
		const Merge20JsonValue* arrays[3] = { begin, end, blank };
		int* fields[3] = { out.begin, out.end, out.blank };
		for (int a = 0; a < 3; ++a)
		{
			if (arrays[a]->type != Merge20JsonValue::Type::Array || arrays[a]->array.size() != 3)
				return false;
			for (int i = 0; i < 3; ++i)
			{
				if (arrays[a]->array[i].type != Merge20JsonValue::Type::Int ||
					arrays[a]->array[i].integer < -1 || arrays[a]->array[i].integer > INT32_MAX)
					return false;
				fields[a][i] = static_cast<int>(arrays[a]->array[i].integer);
			}
		}
		return true;
	}

	// Read one file entry. Returns false when the entry is unusable;
	// the caller skips that file and counts it.
	bool ParseSessionFile(const Merge20JsonValue& value, Merge20SessionFile& out)
	{
		if (value.type != Merge20JsonValue::Type::Object)
			return false;
		const Merge20JsonValue* path = value.Find(L"path");
		if (path == nullptr || path->type != Merge20JsonValue::Type::Text)
			return false;
		out.relPath = path->text;
		int64_t review = 0, action = 0, risk = 0;
		int64_t nMissing = 0, nModified = 0, nAdded = 0, nExcluded = 0;
		int64_t leftSize = -1, rightSize = -1, thirdSize = -1;
		int64_t leftMtime = -1, rightMtime = -1, thirdMtime = -1;
		int64_t nsdiffs = -1, nidiffs = -1;
		if (!GetIntField(value, L"review", review, 0, 4))
			return false;
		if (!GetBoolField(value, L"includeInOutput", out.includeInOutput))
			return false;
		if (!GetBoolField(value, L"classified", out.classified))
			return false;
		if (!GetIntField(value, L"action", action, 0, 6))
			return false;
		if (!GetIntField(value, L"risk", risk, 0, 2))
			return false;
		if (!GetIntField(value, L"nMissing", nMissing, 0, INT32_MAX))
			return false;
		if (!GetIntField(value, L"nModified", nModified, 0, INT32_MAX))
			return false;
		if (!GetIntField(value, L"nAdded", nAdded, 0, INT32_MAX))
			return false;
		if (!GetIntField(value, L"nExcluded", nExcluded, 0, INT32_MAX))
			return false;
		if (!GetTextField(value, L"summary", out.summary))
			return false;
		if (!GetTextField(value, L"memo", out.memo))
			return false;
		if (!GetIntField(value, L"leftSize", leftSize, -1, INT64_MAX))
			return false;
		if (!GetIntField(value, L"rightSize", rightSize, -1, INT64_MAX))
			return false;
		if (!GetIntField(value, L"thirdSize", thirdSize, -1, INT64_MAX))
			return false;
		if (!GetIntField(value, L"leftMtime", leftMtime, -1, INT64_MAX))
			return false;
		if (!GetIntField(value, L"rightMtime", rightMtime, -1, INT64_MAX))
			return false;
		if (!GetIntField(value, L"thirdMtime", thirdMtime, -1, INT64_MAX))
			return false;
		if (!GetIntField(value, L"nsdiffs", nsdiffs, -1, INT32_MAX))
			return false;
		if (!GetIntField(value, L"nidiffs", nidiffs, -1, INT32_MAX))
			return false;
		out.review = static_cast<ReviewState>(review);
		out.action = static_cast<ActionCategory>(action);
		out.risk = static_cast<RiskLevel>(risk);
		out.nMissing = static_cast<int>(nMissing);
		out.nModified = static_cast<int>(nModified);
		out.nAdded = static_cast<int>(nAdded);
		out.nExcluded = static_cast<int>(nExcluded);
		out.leftSize = leftSize;
		out.rightSize = rightSize;
		out.thirdSize = thirdSize;
		out.leftMtime = leftMtime;
		out.rightMtime = rightMtime;
		out.thirdMtime = thirdMtime;
		out.nsdiffs = static_cast<int>(nsdiffs);
		out.nidiffs = static_cast<int>(nidiffs);
		out.diffs.clear();
		const Merge20JsonValue* diffs = value.Find(L"diffs");
		if (diffs != nullptr)
		{
			if (diffs->type != Merge20JsonValue::Type::Array)
				return false;
			for (const auto& diffValue : diffs->array)
			{
				Merge20DiffJudgment judgment;
				if (ParseDiffJudgment(diffValue, judgment))
					out.diffs.push_back(std::move(judgment));
				// A single bad diff is dropped; the file entry survives.
			}
		}
		out.splits.clear();
		const Merge20JsonValue* splits = value.Find(L"splits");
		if (splits != nullptr)
		{
			if (splits->type != Merge20JsonValue::Type::Array)
				return false;
			for (const auto& splitValue : splits->array)
			{
				Merge20SplitGroup group;
				if (ParseSplitGroup(splitValue, group))
					out.splits.push_back(std::move(group));
				// A single bad split group is dropped; the file entry survives.
			}
		}
		return true;
	}

	Merge20JsonValue DiffJudgmentToJson(const Merge20DiffJudgment& judgment)
	{
		std::vector<Merge20JsonValue> begin, end, srcPanes;
		for (int i = 0; i < 3; ++i)
		{
			begin.push_back(Merge20JsonInt(judgment.begin[i]));
			end.push_back(Merge20JsonInt(judgment.end[i]));
		}
		for (int pane : judgment.srcPanes)
			srcPanes.push_back(Merge20JsonInt(pane));
		return Merge20JsonObject({
			{ L"diffIdx", Merge20JsonInt(judgment.diffIdx) },
			{ L"op", Merge20JsonInt(judgment.op) },
			{ L"begin", Merge20JsonArray(std::move(begin)) },
			{ L"end", Merge20JsonArray(std::move(end)) },
			{ L"state", Merge20JsonInt(judgment.state) },
			{ L"srcPanes", Merge20JsonArray(std::move(srcPanes)) },
		});
	}

	Merge20JsonValue SplitGroupToJson(const Merge20SplitGroup& group)
	{
		std::vector<Merge20JsonValue> begin, end, blank;
		for (int i = 0; i < 3; ++i)
		{
			begin.push_back(Merge20JsonInt(group.begin[i]));
			end.push_back(Merge20JsonInt(group.end[i]));
			blank.push_back(Merge20JsonInt(group.blank[i]));
		}
		return Merge20JsonObject({
			{ L"op", Merge20JsonInt(group.op) },
			{ L"begin", Merge20JsonArray(std::move(begin)) },
			{ L"end", Merge20JsonArray(std::move(end)) },
			{ L"dbegin", Merge20JsonInt(group.dbegin) },
			{ L"dend", Merge20JsonInt(group.dend) },
			{ L"blank", Merge20JsonArray(std::move(blank)) },
			{ L"rows", Merge20JsonInt(group.rows) },
		});
	}

	Merge20JsonValue SessionFileToJson(const Merge20SessionFile& file)
	{
		std::vector<Merge20JsonValue> diffs;
		for (const auto& judgment : file.diffs)
			diffs.push_back(DiffJudgmentToJson(judgment));
		std::vector<Merge20JsonValue> splits;
		for (const auto& group : file.splits)
			splits.push_back(SplitGroupToJson(group));
		return Merge20JsonObject({
			{ L"path", Merge20JsonText(file.relPath) },
			{ L"review", Merge20JsonInt(static_cast<int>(file.review)) },
			{ L"includeInOutput", Merge20JsonBool(file.includeInOutput) },
			{ L"classified", Merge20JsonBool(file.classified) },
			{ L"action", Merge20JsonInt(static_cast<int>(file.action)) },
			{ L"risk", Merge20JsonInt(static_cast<int>(file.risk)) },
			{ L"nMissing", Merge20JsonInt(file.nMissing) },
			{ L"nModified", Merge20JsonInt(file.nModified) },
			{ L"nAdded", Merge20JsonInt(file.nAdded) },
			{ L"nExcluded", Merge20JsonInt(file.nExcluded) },
			{ L"summary", Merge20JsonText(file.summary) },
			{ L"memo", Merge20JsonText(file.memo) },
			{ L"leftSize", Merge20JsonInt(file.leftSize) },
			{ L"rightSize", Merge20JsonInt(file.rightSize) },
			{ L"thirdSize", Merge20JsonInt(file.thirdSize) },
			{ L"leftMtime", Merge20JsonInt(file.leftMtime) },
			{ L"rightMtime", Merge20JsonInt(file.rightMtime) },
			{ L"thirdMtime", Merge20JsonInt(file.thirdMtime) },
			{ L"nsdiffs", Merge20JsonInt(file.nsdiffs) },
			{ L"nidiffs", Merge20JsonInt(file.nidiffs) },
			{ L"diffs", Merge20JsonArray(std::move(diffs)) },
			{ L"splits", Merge20JsonArray(std::move(splits)) },
		});
	}

} // namespace

bool Merge20SessionToJson(const Merge20Session& session, std::string& outUtf8)
{
	std::vector<Merge20JsonValue> reviewFilter;
	for (int i = 0; i < 5; ++i)
		reviewFilter.push_back(Merge20JsonBool(session.display.reviewFilter[i]));
	std::vector<Merge20JsonValue> hiddenItems;
	for (const String& item : session.display.hiddenItems)
		hiddenItems.push_back(Merge20JsonText(item));
	std::vector<Merge20JsonValue> files;
	for (const auto& file : session.files)
		files.push_back(SessionFileToJson(file));

	Merge20JsonValue root = Merge20JsonObject({
		{ L"app", Merge20JsonText(L"Merge20") },
		{ L"version", Merge20JsonInt(kMerge20SessionFormatVersion) },
		{ L"savedAt", Merge20JsonText(session.savedAt) },
		{ L"leftPath", Merge20JsonText(session.leftPath) },
		{ L"rightPath", Merge20JsonText(session.rightPath) },
		{ L"thirdPath", Merge20JsonText(session.thirdPath) },
		{ L"compareOptions", Merge20JsonObject({
			{ L"recursive", Merge20JsonBool(session.compareOptions.recursive) },
			{ L"fileFilter", Merge20JsonText(session.compareOptions.fileFilter) },
			{ L"compareMethod", Merge20JsonInt(session.compareOptions.compareMethod) },
			{ L"ignoreWhitespace", Merge20JsonInt(session.compareOptions.ignoreWhitespace) },
			{ L"ignoreCase", Merge20JsonBool(session.compareOptions.ignoreCase) },
			{ L"ignoreBlankLines", Merge20JsonBool(session.compareOptions.ignoreBlankLines) },
			{ L"ignoreEol", Merge20JsonBool(session.compareOptions.ignoreEol) },
		}) },
		{ L"display", Merge20JsonObject({
			{ L"showSkipped", Merge20JsonBool(session.display.showSkipped) },
			{ L"showUniqueLeft", Merge20JsonBool(session.display.showUniqueLeft) },
			{ L"showUniqueMiddle", Merge20JsonBool(session.display.showUniqueMiddle) },
			{ L"showUniqueRight", Merge20JsonBool(session.display.showUniqueRight) },
			{ L"showBinaries", Merge20JsonBool(session.display.showBinaries) },
			{ L"showIdentical", Merge20JsonBool(session.display.showIdentical) },
			{ L"showDifferent", Merge20JsonBool(session.display.showDifferent) },
			{ L"showDifferentLeftOnly", Merge20JsonBool(session.display.showDifferentLeftOnly) },
			{ L"showDifferentMiddleOnly", Merge20JsonBool(session.display.showDifferentMiddleOnly) },
			{ L"showDifferentRightOnly", Merge20JsonBool(session.display.showDifferentRightOnly) },
			{ L"showMissingLeftOnly", Merge20JsonBool(session.display.showMissingLeftOnly) },
			{ L"showMissingMiddleOnly", Merge20JsonBool(session.display.showMissingMiddleOnly) },
			{ L"showMissingRightOnly", Merge20JsonBool(session.display.showMissingRightOnly) },
			{ L"showEmptyFolders", Merge20JsonBool(session.display.showEmptyFolders) },
			{ L"treeMode", Merge20JsonBool(session.display.treeMode) },
			{ L"reviewFilter", Merge20JsonArray(std::move(reviewFilter)) },
			{ L"columnOrders", Merge20JsonText(session.display.columnOrders) },
			{ L"columnWidths", Merge20JsonText(session.display.columnWidths) },
			{ L"hiddenItems", Merge20JsonArray(std::move(hiddenItems)) },
			{ L"focusedPath", Merge20JsonText(session.display.focusedPath) },
			{ L"topIndex", Merge20JsonInt(session.display.topIndex) },
		}) },
		{ L"files", Merge20JsonArray(std::move(files)) },
	});
	outUtf8 = Merge20JsonStringify(root);
	return true;
}

bool Merge20SessionFromJson(const std::string& utf8, Merge20Session& out, std::string& errDetail, int& skippedFiles)
{
	skippedFiles = 0;
	errDetail.clear();
	Merge20JsonValue root;
	if (!Merge20JsonParse(utf8, root, errDetail))
		return false;
	if (root.type != Merge20JsonValue::Type::Object)
	{
		errDetail = "top level is not an object";
		return false;
	}
	const Merge20JsonValue* app = root.Find(L"app");
	if (app == nullptr || app->type != Merge20JsonValue::Type::Text || app->text != L"Merge20")
	{
		errDetail = "not a Merge20 session file";
		return false;
	}
	const Merge20JsonValue* version = root.Find(L"version");
	if (version == nullptr || version->type != Merge20JsonValue::Type::Int)
	{
		errDetail = "missing session version";
		return false;
	}
	if (version->integer != kMerge20SessionFormatVersion)
	{
		errDetail = "unsupported session version " + std::to_string(version->integer);
		return false;
	}
	// From here on, a wrong-typed field fails the load with this reason
	// (a single bad file entry is still only skipped, see below).
	errDetail = "invalid field in session file";

	Merge20Session session;
	if (!GetTextField(root, L"savedAt", session.savedAt))
		return false;
	if (!GetTextField(root, L"leftPath", session.leftPath))
		return false;
	if (!GetTextField(root, L"rightPath", session.rightPath))
		return false;
	if (!GetTextField(root, L"thirdPath", session.thirdPath))
		return false;

	const Merge20JsonValue* compareOptions = root.Find(L"compareOptions");
	if (compareOptions != nullptr)
	{
		if (compareOptions->type != Merge20JsonValue::Type::Object)
			return false;
		int64_t compareMethod = 0, ignoreWhitespace = 1;
		if (!GetBoolField(*compareOptions, L"recursive", session.compareOptions.recursive))
			return false;
		if (!GetTextField(*compareOptions, L"fileFilter", session.compareOptions.fileFilter))
			return false;
		if (!GetIntField(*compareOptions, L"compareMethod", compareMethod, 0, INT32_MAX))
			return false;
		if (!GetIntField(*compareOptions, L"ignoreWhitespace", ignoreWhitespace, 0, 2))
			return false;
		if (!GetBoolField(*compareOptions, L"ignoreCase", session.compareOptions.ignoreCase))
			return false;
		if (!GetBoolField(*compareOptions, L"ignoreBlankLines", session.compareOptions.ignoreBlankLines))
			return false;
		if (!GetBoolField(*compareOptions, L"ignoreEol", session.compareOptions.ignoreEol))
			return false;
		session.compareOptions.compareMethod = static_cast<int>(compareMethod);
		session.compareOptions.ignoreWhitespace = static_cast<int>(ignoreWhitespace);
	}

	const Merge20JsonValue* display = root.Find(L"display");
	if (display != nullptr)
	{
		if (display->type != Merge20JsonValue::Type::Object)
			return false;
		int64_t topIndex = 0;
		if (!GetBoolField(*display, L"showSkipped", session.display.showSkipped))
			return false;
		if (!GetBoolField(*display, L"showUniqueLeft", session.display.showUniqueLeft))
			return false;
		if (!GetBoolField(*display, L"showUniqueMiddle", session.display.showUniqueMiddle))
			return false;
		if (!GetBoolField(*display, L"showUniqueRight", session.display.showUniqueRight))
			return false;
		if (!GetBoolField(*display, L"showBinaries", session.display.showBinaries))
			return false;
		if (!GetBoolField(*display, L"showIdentical", session.display.showIdentical))
			return false;
		if (!GetBoolField(*display, L"showDifferent", session.display.showDifferent))
			return false;
		if (!GetBoolField(*display, L"showDifferentLeftOnly", session.display.showDifferentLeftOnly))
			return false;
		if (!GetBoolField(*display, L"showDifferentMiddleOnly", session.display.showDifferentMiddleOnly))
			return false;
		if (!GetBoolField(*display, L"showDifferentRightOnly", session.display.showDifferentRightOnly))
			return false;
		if (!GetBoolField(*display, L"showMissingLeftOnly", session.display.showMissingLeftOnly))
			return false;
		if (!GetBoolField(*display, L"showMissingMiddleOnly", session.display.showMissingMiddleOnly))
			return false;
		if (!GetBoolField(*display, L"showMissingRightOnly", session.display.showMissingRightOnly))
			return false;
		if (!GetBoolField(*display, L"showEmptyFolders", session.display.showEmptyFolders))
			return false;
		if (!GetBoolField(*display, L"treeMode", session.display.treeMode))
			return false;
		if (!GetTextField(*display, L"columnOrders", session.display.columnOrders))
			return false;
		if (!GetTextField(*display, L"columnWidths", session.display.columnWidths))
			return false;
		if (!GetTextField(*display, L"focusedPath", session.display.focusedPath))
			return false;
		if (!GetIntField(*display, L"topIndex", topIndex, 0, INT32_MAX))
			return false;
		session.display.topIndex = static_cast<int>(topIndex);
		const Merge20JsonValue* reviewFilter = display->Find(L"reviewFilter");
		if (reviewFilter != nullptr)
		{
			if (reviewFilter->type != Merge20JsonValue::Type::Array || reviewFilter->array.size() != 5)
				return false;
			for (int i = 0; i < 5; ++i)
			{
				if (reviewFilter->array[i].type != Merge20JsonValue::Type::Bool)
					return false;
				session.display.reviewFilter[i] = reviewFilter->array[i].boolean;
			}
		}
		const Merge20JsonValue* hiddenItems = display->Find(L"hiddenItems");
		if (hiddenItems != nullptr)
		{
			if (hiddenItems->type != Merge20JsonValue::Type::Array)
				return false;
			for (const auto& item : hiddenItems->array)
			{
				if (item.type != Merge20JsonValue::Type::Text)
					return false;
				session.display.hiddenItems.push_back(item.text);
			}
		}
	}

	const Merge20JsonValue* files = root.Find(L"files");
	if (files != nullptr)
	{
		if (files->type != Merge20JsonValue::Type::Array)
			return false;
		for (const auto& fileValue : files->array)
		{
			Merge20SessionFile file;
			if (ParseSessionFile(fileValue, file))
				session.files.push_back(std::move(file));
			else
				++skippedFiles;
		}
	}

	out = std::move(session);
	errDetail.clear();
	return true;
}

bool Merge20SessionSaveToFile(const Merge20Session& session, const String& filePath, std::string& errDetail)
{
	errDetail.clear();
	std::string utf8;
	if (!Merge20SessionToJson(session, utf8))
	{
		errDetail = "failed to serialize session";
		return false;
	}
	std::filesystem::path target(filePath);
	// The ".tmp" suffix is ASCII, so building the temp path from the
	// String name is safe on both Windows (native wide path) and
	// POSIX (codecvt conversion of an ASCII name).
	String tmpName = filePath;
	tmpName += L".tmp";
	std::filesystem::path tmp(tmpName);
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			errDetail = "cannot create temporary file";
			return false;
		}
		out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
		out.flush();
		if (!out)
		{
			errDetail = "failed to write session file";
			std::error_code ec;
			std::filesystem::remove(tmp, ec);
			return false;
		}
	}
	std::error_code ec;
	std::filesystem::rename(tmp, target, ec);
	if (ec)
	{
		// Rename failed (e.g. a locked target): fall back to a direct
		// write so the save still has a chance to succeed.
		std::ofstream out(target, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			errDetail = "cannot create session file";
			std::filesystem::remove(tmp, ec);
			return false;
		}
		out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
		out.flush();
		if (!out)
		{
			errDetail = "failed to write session file";
			std::filesystem::remove(tmp, ec);
			return false;
		}
		std::filesystem::remove(tmp, ec);
	}
	return true;
}

bool Merge20SessionLoadFromFile(const String& filePath, Merge20Session& out, std::string& errDetail, int& skippedFiles)
{
	skippedFiles = 0;
	errDetail.clear();
	std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
	if (!in)
	{
		errDetail = "cannot open session file";
		return false;
	}
	std::string utf8((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (in.bad())
	{
		errDetail = "failed to read session file";
		return false;
	}
	return Merge20SessionFromJson(utf8, out, errDetail, skippedFiles);
}
