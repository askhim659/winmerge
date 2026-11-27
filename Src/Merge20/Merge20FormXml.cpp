/**
 * @file  Merge20FormXml.cpp
 *
 * @brief Implementation of the hand-written FORM XML parser (F0).
 *
 * Port of the HTML tool's `parseXmlToMapAndShapes` (FROM_____1.html):
 * the element walk, the HTML key rule, and the shape extraction
 * (x/y/x2/y2/point fallbacks and the estimation rule) are the same.
 * The DOM walk is replaced by a small recursive-descent parser
 * because WinMerge has no XML DOM library in use (design section
 * 3.3: no MSXML, no Poco XML).
 *
 * This file stays ASCII: no Japanese literal is needed here (all
 * user-visible words are composed by the caller through `_()`).
 */
#include "pch.h"
#include <cmath>
#include <limits>
#include <unordered_set>
#include "Merge20/Merge20FormXml.h"

namespace
{

	// ASCII whitespace (plus the two non-ASCII spaces JavaScript
	// String trimming also removes: U+00A0 and U+FEFF).
	bool IsSpace(wchar_t c)
	{
		return c == L' ' || c == L'\t' || c == L'\n' || c == L'\v' || c == L'\f' || c == L'\r' ||
			c == 0x00A0 || c == 0xFEFF;
	}

	bool IsNameStart(wchar_t c)
	{
		return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'_' || c == L':' || c >= 0x80;
	}

	bool IsNameChar(wchar_t c)
	{
		return IsNameStart(c) || (c >= L'0' && c <= L'9') || c == L'-' || c == L'.';
	}

	bool IsDigit(wchar_t c)
	{
		return c >= L'0' && c <= L'9';
	}

	bool IsHexDigit(wchar_t c)
	{
		return IsDigit(c) || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
	}

	int HexValue(wchar_t c)
	{
		if (c >= L'0' && c <= L'9')
			return c - L'0';
		if (c >= L'a' && c <= L'f')
			return c - L'a' + 10;
		return c - L'A' + 10;
	}

	// Lower-case ASCII letters only (enough for the `line` check;
	// non-ASCII letters are left unchanged).
	std::wstring ToLowerAscii(const std::wstring& text)
	{
		std::wstring lower = text;
		for (wchar_t& c : lower)
		{
			if (c >= L'A' && c <= L'Z')
				c = static_cast<wchar_t>(c - L'A' + L'a');
		}
		return lower;
	}

	// XML 1.0 valid characters (Char production, without the
	// surrogate range, which never appears in a wchar_t string
	// built from a decoded file).
	bool IsValidXmlChar(unsigned long cp)
	{
		return cp == 0x9 || cp == 0xA || cp == 0xD ||
			(cp >= 0x20 && cp <= 0xD7FF) ||
			(cp >= 0xE000 && cp <= 0xFFFD) ||
			(cp >= 0x10000 && cp <= 0x10FFFF);
	}

	// One parsed node: the element plus its child elements. The
	// parse builds this tree first and flattens it in document
	// order afterwards, so that the element text (known only after
	// the children are read) is complete when the shape is built.
	struct FormXmlNode
	{
		Merge20FormElement element;
		std::vector<FormXmlNode> children;
	};

	class FormXmlParser
	{
	public:
		explicit FormXmlParser(const std::wstring& xml)
			: m_xml(xml)
		{
		}

		bool Parse(Merge20FormParseResult& result)
		{
			// A leading byte-order mark is skipped (a decoded file
			// may still carry U+FEFF at the start).
			if (m_pos < m_xml.size() && m_xml[m_pos] == 0xFEFF)
				++m_pos;
			if (!SkipMisc())
				return false;
			if (m_pos >= m_xml.size())
				return false; // No root element.
			FormXmlNode root;
			if (!ParseElement(L"", 0, root))
				return false;
			if (!SkipMisc())
				return false;
			if (m_pos < m_xml.size())
				return false; // Trailing content after the root.
			std::unordered_set<std::wstring> usedKeys;
			int dupCounter = 0;
			Flatten(root, result, usedKeys, dupCounter);
			result.ok = true;
			return true;
		}

	private:
		const std::wstring& m_xml;
		size_t m_pos = 0;

		bool AtEnd() const { return m_pos >= m_xml.size(); }

		wchar_t Peek(size_t off = 0) const
		{
			return m_pos + off < m_xml.size() ? m_xml[m_pos + off] : L'\0';
		}

		bool StartsWith(const wchar_t* text) const
		{
			const size_t len = std::char_traits<wchar_t>::length(text);
			return m_pos + len <= m_xml.size() && m_xml.compare(m_pos, len, text) == 0;
		}

		void SkipSpaces()
		{
			while (!AtEnd() && IsSpace(m_xml[m_pos]))
				++m_pos;
		}

		// Whitespace, comments, processing instructions and the
		// DOCTYPE declaration before/after the root element.
		bool SkipMisc()
		{
			for (;;)
			{
				SkipSpaces();
				if (StartsWith(L"<!--"))
				{
					if (!ParseComment())
						return false;
				}
				else if (StartsWith(L"<?"))
				{
					if (!ParseProcessingInstruction())
						return false;
				}
				else if (StartsWith(L"<!DOCTYPE"))
				{
					if (!ParseDoctype())
						return false;
				}
				else
				{
					return true;
				}
			}
		}

		bool ParseComment()
		{
			// m_pos is at "<!--".
			const size_t end = m_xml.find(L"-->", m_pos + 4);
			if (end == std::wstring::npos)
				return false;
			m_pos = end + 3;
			return true;
		}

		bool ParseProcessingInstruction()
		{
			// m_pos is at "<?" (this also skips the XML declaration).
			const size_t end = m_xml.find(L"?>", m_pos + 2);
			if (end == std::wstring::npos)
				return false;
			m_pos = end + 2;
			return true;
		}

		bool ParseDoctype()
		{
			// m_pos is at "<!DOCTYPE". The internal subset ([...])
			// may contain '>', so the bracket depth is tracked.
			// Quoted strings inside are skipped as well.
			int depth = 0;
			while (!AtEnd())
			{
				const wchar_t c = m_xml[m_pos++];
				if (c == L'"' || c == L'\'')
				{
					while (!AtEnd() && m_xml[m_pos] != c)
						++m_pos;
					if (AtEnd())
						return false;
					++m_pos;
				}
				else if (c == L'[')
				{
					++depth;
				}
				else if (c == L']')
				{
					--depth;
				}
				else if (c == L'>' && depth <= 0)
				{
					return true;
				}
			}
			return false;
		}

		bool ParseName(std::wstring& name)
		{
			if (AtEnd() || !IsNameStart(m_xml[m_pos]))
				return false;
			const size_t start = m_pos;
			while (!AtEnd() && IsNameChar(m_xml[m_pos]))
				++m_pos;
			name = m_xml.substr(start, m_pos - start);
			return true;
		}

		// `&...;` at m_pos. On success m_pos is past the ';' and the
		// decoded (or kept-as-is) text is appended to `out`.
		bool DecodeEntity(std::wstring& out)
		{
			// m_xml[m_pos] == '&'.
			const size_t semi = m_xml.find(L';', m_pos);
			if (semi == std::wstring::npos)
				return false;
			for (size_t i = m_pos + 1; i < semi; ++i)
			{
				const wchar_t c = m_xml[i];
				if (c == L'<' || c == L'&' || IsSpace(c))
					return false; // Bare '&' or an unterminated reference.
			}
			const std::wstring name = m_xml.substr(m_pos + 1, semi - m_pos - 1);
			if (name == L"amp")
				out.push_back(L'&');
			else if (name == L"lt")
				out.push_back(L'<');
			else if (name == L"gt")
				out.push_back(L'>');
			else if (name == L"quot")
				out.push_back(L'"');
			else if (name == L"apos")
				out.push_back(L'\'');
			else if (name.size() > 1 && name[0] == L'#')
			{
				unsigned long cp = 0;
				if (name[1] == L'x' || name[1] == L'X')
				{
					if (name.size() < 3)
						return false;
					for (size_t i = 2; i < name.size(); ++i)
					{
						if (!IsHexDigit(name[i]))
							return false;
						cp = cp * 16 + static_cast<unsigned long>(HexValue(name[i]));
						if (cp > 0x10FFFF)
							return false;
					}
				}
				else
				{
					for (size_t i = 1; i < name.size(); ++i)
					{
						if (!IsDigit(name[i]))
							return false;
						cp = cp * 10 + static_cast<unsigned long>(name[i] - L'0');
						if (cp > 0x10FFFF)
							return false;
					}
				}
				if (!IsValidXmlChar(cp))
					return false;
				if (cp <= 0xFFFF)
				{
					out.push_back(static_cast<wchar_t>(cp));
				}
				else if (sizeof(wchar_t) >= 4)
				{
					out.push_back(static_cast<wchar_t>(cp));
				}
				else
				{
					// UTF-16 surrogate pair (Windows wchar_t).
					cp -= 0x10000;
					out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
					out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
				}
			}
			else if (name.empty())
			{
				return false; // "&;" is not a reference.
			}
			else
			{
				// Undefined entity: kept as-is (design section 3.3).
				out.append(m_xml, m_pos, semi - m_pos + 1);
			}
			m_pos = semi + 1;
			return true;
		}

		// Attribute value after the opening quote. Attribute-value
		// normalisation (XML spec): a literal tab / line feed /
		// carriage return becomes one space (CRLF counts as one).
		// A character reference keeps its character as-is.
		bool ParseAttributeValue(wchar_t quote, std::wstring& value)
		{
			while (!AtEnd())
			{
				const wchar_t c = m_xml[m_pos];
				if (c == quote)
				{
					++m_pos;
					return true;
				}
				if (c == L'<')
					return false;
				if (c == L'&')
				{
					if (!DecodeEntity(value))
						return false;
				}
				else if (c == L'\r')
				{
					++m_pos;
					if (!AtEnd() && m_xml[m_pos] == L'\n')
						++m_pos;
					value.push_back(L' ');
				}
				else if (c == L'\n' || c == L'\t')
				{
					++m_pos;
					value.push_back(L' ');
				}
				else
				{
					value.push_back(c);
					++m_pos;
				}
			}
			return false; // Unclosed quote.
		}

		// Tag name at m_pos + 1 without consuming input (used to
		// count same-tag siblings before parsing the child).
		std::wstring PeekTagName() const
		{
			size_t i = m_pos + 1;
			if (i >= m_xml.size() || !IsNameStart(m_xml[i]))
				return std::wstring();
			const size_t start = i;
			while (i < m_xml.size() && IsNameChar(m_xml[i]))
				++i;
			return m_xml.substr(start, i - start);
		}

		bool ParseElement(const std::wstring& parentPath, int siblingIndex, FormXmlNode& node)
		{
			// m_pos is at '<'.
			++m_pos;
			std::wstring tag;
			if (!ParseName(tag))
				return false;
			node.element.tag = tag;
			node.element.structPath = parentPath.empty()
				? L"/" + tag
				: parentPath + L"/" + tag + L"[" + std::to_wstring(siblingIndex) + L"]";

			bool empty = false;
			for (;;)
			{
				SkipSpaces();
				if (AtEnd())
					return false;
				if (Peek() == L'>')
				{
					++m_pos;
					break;
				}
				if (Peek() == L'/' && Peek(1) == L'>')
				{
					m_pos += 2;
					empty = true;
					break;
				}
				std::wstring attrName;
				if (!ParseName(attrName))
					return false;
				SkipSpaces();
				if (Peek() != L'=')
					return false;
				++m_pos;
				SkipSpaces();
				const wchar_t quote = Peek();
				if (quote != L'"' && quote != L'\'')
					return false;
				++m_pos;
				std::wstring attrValue;
				if (!ParseAttributeValue(quote, attrValue))
					return false; // Unclosed quote.
				if (node.element.attrs.find(attrName) != node.element.attrs.end())
					return false; // Duplicate attribute.
				node.element.attrs[attrName] = attrValue;
			}

			const std::map<std::wstring, std::wstring>::const_iterator nameIt =
				node.element.attrs.find(L"name");
			node.element.name = nameIt != node.element.attrs.end() ? nameIt->second : std::wstring();
			if (empty)
				return true;

			std::map<std::wstring, int> childCounts;
			for (;;)
			{
				if (AtEnd())
					return false; // Unclosed element.
				if (StartsWith(L"</"))
				{
					m_pos += 2;
					SkipSpaces();
					std::wstring endTag;
					if (!ParseName(endTag))
						return false;
					SkipSpaces();
					if (Peek() != L'>')
						return false;
					++m_pos;
					return endTag == tag;
				}
				if (StartsWith(L"<!--"))
				{
					if (!ParseComment())
						return false;
				}
				else if (StartsWith(L"<![CDATA["))
				{
					m_pos += 9;
					const size_t end = m_xml.find(L"]]>", m_pos);
					if (end == std::wstring::npos)
						return false;
					node.element.text.append(m_xml, m_pos, end - m_pos);
					m_pos = end + 3;
				}
				else if (StartsWith(L"<?"))
				{
					if (!ParseProcessingInstruction())
						return false;
				}
				else if (Peek() == L'<')
				{
					const std::wstring childTag = PeekTagName();
					if (childTag.empty())
						return false;
					const int index = ++childCounts[childTag];
					FormXmlNode child;
					if (!ParseElement(node.element.structPath, index, child))
						return false;
					node.children.push_back(std::move(child));
				}
				else
				{
					// Text run (until the next '<'), one character
					// at a time so that entity references are
					// decoded inline.
					while (!AtEnd() && m_xml[m_pos] != L'<')
					{
						if (StartsWith(L"]]>"))
							return false; // "]]>" must not appear in content.
						if (m_xml[m_pos] == L'&')
						{
							if (!DecodeEntity(node.element.text))
								return false;
						}
						else
						{
							node.element.text.push_back(m_xml[m_pos]);
							++m_pos;
						}
					}
				}
			}
		}

		// HTML key rule (design section 2.2): tag[name='...'] when a
		// name attribute exists, otherwise the attribute signature.
		// xmlns declarations are excluded from the signature
		// (design section 3.3); the signature lists attributes in
		// sorted order (std::map), unlike the HTML document order.
		std::wstring BuildKey(const Merge20FormElement& element) const
		{
			if (!element.name.empty())
				return element.tag + L"[name='" + element.name + L"']";
			std::wstring signature;
			bool first = true;
			for (const std::pair<const std::wstring, std::wstring>& attr : element.attrs)
			{
				if (attr.first == L"xmlns" || attr.first.compare(0, 6, L"xmlns:") == 0)
					continue;
				if (!first)
					signature += L",";
				first = false;
				signature += attr.first + L"=" + attr.second;
			}
			return element.tag + L"[" + signature + L"]";
		}

		// HTML shape extraction (design section 2.2).
		bool BuildShape(const Merge20FormElement& element, Merge20FormShape& shape) const
		{
			const std::map<std::wstring, std::wstring>& attrs = element.attrs;
			const std::wstring* xText = FindAttr(attrs, L"x", L"left", L"x1");
			const std::wstring* yText = FindAttr(attrs, L"y", L"top", L"y1");
			double x = std::numeric_limits<double>::quiet_NaN();
			double y = std::numeric_limits<double>::quiet_NaN();
			if (xText != nullptr)
			{
				double v = 0.0;
				x = Merge20FormParseNumber(*xText, v) ? v : std::numeric_limits<double>::quiet_NaN();
			}
			if (yText != nullptr)
			{
				double v = 0.0;
				y = Merge20FormParseNumber(*yText, v) ? v : std::numeric_limits<double>::quiet_NaN();
			}
			if (std::isnan(x) || std::isnan(y))
				return false;

			double x2 = std::numeric_limits<double>::quiet_NaN();
			double y2 = std::numeric_limits<double>::quiet_NaN();
			const std::wstring* x2Text = FindAttr(attrs, L"x2", nullptr, nullptr);
			const std::wstring* widthText = FindAttr(attrs, L"width", nullptr, nullptr);
			if (x2Text != nullptr)
			{
				double v = 0.0;
				x2 = Merge20FormParseNumber(*x2Text, v) ? v : std::numeric_limits<double>::quiet_NaN();
			}
			else if (widthText != nullptr)
			{
				double w = 0.0;
				x2 = Merge20FormParseNumber(*widthText, w) ? x + w : std::numeric_limits<double>::quiet_NaN();
			}
			const std::wstring* y2Text = FindAttr(attrs, L"y2", nullptr, nullptr);
			const std::wstring* heightText = FindAttr(attrs, L"height", nullptr, nullptr);
			if (y2Text != nullptr)
			{
				double v = 0.0;
				y2 = Merge20FormParseNumber(*y2Text, v) ? v : std::numeric_limits<double>::quiet_NaN();
			}
			else if (heightText != nullptr)
			{
				double h = 0.0;
				y2 = Merge20FormParseNumber(*heightText, h) ? y + h : std::numeric_limits<double>::quiet_NaN();
			}

			double point = 10.0;
			const std::wstring* pointText = FindAttr(attrs, L"point", L"fontsize", nullptr);
			const std::wstring* fontText = FindAttr(attrs, L"fontname", L"font", L"fontName");
			if (pointText != nullptr)
			{
				double v = 0.0;
				point = Merge20FormParseNumber(*pointText, v) ? v : std::numeric_limits<double>::quiet_NaN();
			}

			// Estimated size (HTML rule): the name length is the
			// `name` attribute length, or 4 when there is no name.
			const double nameLen = element.name.empty() ? 4.0 : static_cast<double>(element.name.length());
			const bool estimated = std::isnan(x2) || std::isnan(y2);
			if (std::isnan(x2))
			{
				x2 = std::isnan(point) ? std::numeric_limits<double>::quiet_NaN()
					: x + (std::max)(80.0, nameLen * point * 1.5);
			}
			if (std::isnan(y2))
			{
				y2 = std::isnan(point) ? std::numeric_limits<double>::quiet_NaN()
					: y + (std::max)(24.0, point * 2.0);
			}

			shape.key = element.key;
			shape.tag = element.tag;
			shape.name = element.name.empty() ? element.tag : element.name;
			shape.x = x;
			shape.y = y;
			shape.x2 = x2;
			shape.y2 = y2;
			shape.point = point;
			shape.fontName = (fontText != nullptr) ? *fontText : std::wstring();
			shape.isLine = ToLowerAscii(element.tag).find(L"line") != std::wstring::npos;
			shape.text = element.text;
			shape.isEstimated = estimated;
			return true;
		}

		const std::wstring* FindAttr(const std::map<std::wstring, std::wstring>& attrs,
			const wchar_t* first, const wchar_t* second, const wchar_t* third) const
		{
			const wchar_t* names[3] = { first, second, third };
			for (int i = 0; i < 3; ++i)
			{
				if (names[i] == nullptr)
					continue;
				const std::map<std::wstring, std::wstring>::const_iterator it = attrs.find(names[i]);
				if (it != attrs.end())
					return &it->second;
			}
			return nullptr;
		}

		// Pre-order flattening (same order as the HTML walk):
		// the element and its shape come before the children.
		void Flatten(const FormXmlNode& node, Merge20FormParseResult& result,
			std::unordered_set<std::wstring>& usedKeys, int& dupCounter)
		{
			Merge20FormElement element = node.element;
			std::wstring key = BuildKey(element);
			if (usedKeys.find(key) != usedKeys.end())
			{
				++dupCounter;
				key += L"#" + std::to_wstring(dupCounter);
			}
			usedKeys.insert(key);
			element.key = key;
			Merge20FormShape shape;
			const bool hasShape = BuildShape(element, shape);
			result.elements.push_back(element);
			if (hasShape)
				result.shapes.push_back(shape);
			for (const FormXmlNode& child : node.children)
				Flatten(child, result, usedKeys, dupCounter);
		}
	};

	// Manual decimal parse (locale-independent): the decimal point
	// is always '.'. Used by Merge20FormParseNumber for the mantissa.
	bool ParseDecimal(const wchar_t* begin, const wchar_t* end, double& value)
	{
		// [digits] [. digits] | . digits, then an optional exponent.
		double mantissa = 0.0;
		bool anyDigit = false;
		const wchar_t* p = begin;
		while (p < end && IsDigit(*p))
		{
			mantissa = mantissa * 10.0 + (*p - L'0');
			++p;
			anyDigit = true;
		}
		if (p < end && *p == L'.')
		{
			++p;
			double frac = 0.0;
			double scale = 1.0;
			while (p < end && IsDigit(*p))
			{
				frac = frac * 10.0 + (*p - L'0');
				scale *= 10.0;
				++p;
				anyDigit = true;
			}
			mantissa += frac / scale;
		}
		if (!anyDigit)
			return false;
		if (p < end && (*p == L'e' || *p == L'E'))
		{
			++p;
			bool negExp = false;
			if (p < end && (*p == L'+' || *p == L'-'))
			{
				negExp = (*p == L'-');
				++p;
			}
			if (p >= end || !IsDigit(*p))
				return false;
			int exp = 0;
			while (p < end && IsDigit(*p))
			{
				exp = exp * 10 + (*p - L'0');
				if (exp > 10000)
					return false; // Absurd exponent; treat as invalid.
				++p;
			}
			const double factor = std::pow(10.0, negExp ? -exp : exp);
			mantissa *= factor;
		}
		if (p != end)
			return false;
		value = mantissa;
		return true;
	}

	// Case-insensitive ASCII compare of a trimmed range with a word.
	bool EqualsWordIgnoreCase(const wchar_t* begin, const wchar_t* end, const wchar_t* word)
	{
		while (begin < end && *word != L'\0')
		{
			wchar_t c = *begin;
			if (c >= L'A' && c <= L'Z')
				c = static_cast<wchar_t>(c - L'A' + L'a');
			if (c != *word)
				return false;
			++begin;
			++word;
		}
		return begin == end && *word == L'\0';
	}

} // namespace

bool Merge20FormParseNumber(const std::wstring& text, double& value)
{
	size_t begin = 0;
	while (begin < text.size() && IsSpace(text[begin]))
		++begin;
	size_t end = text.size();
	while (end > begin && IsSpace(text[end - 1]))
		--end;
	if (begin == end)
	{
		value = 0.0; // JavaScript Number("") is 0.
		return true;
	}
	const wchar_t* start = text.c_str() + begin;
	const wchar_t* stop = text.c_str() + end;
	if (EqualsWordIgnoreCase(start, stop, L"infinity"))
	{
		value = std::numeric_limits<double>::infinity();
		return true;
	}
	if (EqualsWordIgnoreCase(start, stop, L"nan"))
	{
		value = std::numeric_limits<double>::quiet_NaN();
		return true;
	}
	bool negative = false;
	const wchar_t* p = start;
	if (p < stop && (*p == L'+' || *p == L'-'))
	{
		negative = (*p == L'-');
		++p;
	}
	if (p < stop && *p == L'0' && p + 1 < stop && (p[1] == L'x' || p[1] == L'X'))
	{
		// Hex integer (JavaScript Number("0x10") is 16).
		p += 2;
		if (p >= stop || !IsHexDigit(*p))
			return false;
		unsigned long v = 0;
		while (p < stop && IsHexDigit(*p))
		{
			v = v * 16 + static_cast<unsigned long>(HexValue(*p));
			++p;
		}
		if (p != stop)
			return false;
		value = negative ? -static_cast<double>(v) : static_cast<double>(v);
		return true;
	}
	double v = 0.0;
	if (!ParseDecimal(p, stop, v))
		return false;
	value = negative ? -v : v;
	return true;
}

Merge20FormParseResult Merge20ParseFormXml(const std::wstring& xml)
{
	Merge20FormParseResult result;
	FormXmlParser parser(xml);
	parser.Parse(result);
	return result;
}

