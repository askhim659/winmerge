/**
 * @file Merge20FormXml_test.cpp
 *
 * @brief Merge20 (Wave 5 F0) unit tests for the FORM XML parser and
 * data model.
 *
 * The form_basic / ns_entities / est_and_line / nested_dup_names
 * cases use the ground truth produced by running the HTML tool's
 * own `parseXmlToMapAndShapes` (FROM_____1.html) in Node with
 * jsdom's DOMParser: element sets, attributes and shape coordinates
 * must match (design section 10 F0 (c)). Key strings are NOT
 * compared against the HTML keys where the fork deliberately
 * differs: the attribute-signature key lists attributes in sorted
 * order (HTML: document order), and `xmlns` / `xmlns:*` attributes
 * are excluded from the signature key (design section 3.3).
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20Classify_test.cpp.
 */
#include "pch.h"
#include <cmath>
#include <gtest/gtest.h>
#include "Merge20/Merge20FormXml.h"

namespace
{

	const Merge20FormElement* FindElement(const Merge20FormParseResult& result, const wchar_t* key)
	{
		for (const Merge20FormElement& element : result.elements)
		{
			if (element.key == key)
				return &element;
		}
		return nullptr;
	}

	const Merge20FormShape* FindShape(const Merge20FormParseResult& result, const wchar_t* key)
	{
		for (const Merge20FormShape& shape : result.shapes)
		{
			if (shape.key == key)
				return &shape;
		}
		return nullptr;
	}

	// HTML ground truth (form_basic): 8 elements, 5 shapes.
	const wchar_t* const kFormBasicXml =
		L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		L"<!-- form definition -->\n"
		L"<form name=\"ZEB001R007\" width=\"1000\" height=\"800\">\n"
		L"  <page name=\"page1\">\n"
		L"    <label name=\"title\" x=\"10\" y=\"20\" width=\"200\" height=\"30\" point=\"14\">"
		L"\u8cc7\u7523\u8a3c\u660e\u66f8</label>\n"
		L"    <field name=\"amount\" x=\"10\" y=\"60\" x2=\"210\" y2=\"90\" fontsize=\"12\">12345</field>\n"
		L"    <line name=\"rule1\" x1=\"0\" y1=\"100\" x2=\"1000\" y2=\"100\" />\n"
		L"    <box left=\"5\" top=\"120\" width=\"300\" height=\"40\" />\n"
		L"    <text><![CDATA[a & b <c>]]></text>\n"
		L"  </page>\n"
		L"  <label name=\"title\" x=\"10\" y=\"200\">dup</label>\n"
		L"</form>\n";

	// HTML ground truth (ns_entities): 4 elements, 2 shapes.
	const wchar_t* const kNamespacesXml =
		L"<?xml version='1.0'?>\n"
		L"<fx:form xmlns:fx=\"http://example.com/fx\" xmlns=\"http://example.com/d\">\n"
		L"  <fx:item name='a&amp;b' x='1.5' y='2.5' point='9'>x &lt; y &quot;q&quot; &#65;&#x42;</fx:item>\n"
		L"  <fx:item x=\"3\" y=\"4\" />\n"
		L"  <plain>no coords here</plain>\n"
		L"</fx:form>\n";

	// HTML ground truth (est_and_line): 5 elements, 4 shapes.
	const wchar_t* const kEstimationXml =
		L"<form>"
		L"<field name=\"ab\" x=\"50\" y=\"70\" />"
		L"<field x=\"10\" y=\"20\" width=\"30\" />"
		L"<myline x=\"0\" y=\"0\" x2=\"5\" />"
		L"<UPPERLINE x=\"1\" y=\"1\" x2=\"2\" y2=\"2\" />"
		L"</form>";

	// HTML ground truth (nested_dup_names): 5 elements, 2 shapes.
	const wchar_t* const kDuplicateNamesXml =
		L"<r><g name=\"n\"><i name=\"x\" x=\"1\" y=\"1\" /></g>"
		L"<g name=\"n\"><i name=\"x\" x=\"2\" y=\"2\" /></g></r>";

} // namespace

TEST(Merge20FormXml, ParseBasicForm)
{
	const Merge20FormParseResult result = Merge20ParseFormXml(kFormBasicXml);
	ASSERT_TRUE(result.ok);
	ASSERT_EQ(result.elements.size(), static_cast<size_t>(8));
	ASSERT_EQ(result.shapes.size(), static_cast<size_t>(5));

	const Merge20FormElement* form = FindElement(result, L"form[name='ZEB001R007']");
	ASSERT_NE(form, nullptr);
	EXPECT_EQ(form->tag, std::wstring(L"form"));
	EXPECT_EQ(form->name, std::wstring(L"ZEB001R007"));
	EXPECT_EQ(form->structPath, std::wstring(L"/form"));
	EXPECT_EQ(form->attrs.size(), static_cast<size_t>(3));

	const Merge20FormElement* label = FindElement(result, L"label[name='title']");
	ASSERT_NE(label, nullptr);
	EXPECT_EQ(label->structPath, std::wstring(L"/form/page[1]/label[1]"));
	EXPECT_EQ(label->attrs.size(), static_cast<size_t>(6));
	EXPECT_EQ(label->attrs.at(L"point"), std::wstring(L"14"));
	EXPECT_EQ(label->text, std::wstring(L"\u8cc7\u7523\u8a3c\u660e\u66f8"));

	const Merge20FormElement* text = FindElement(result, L"text[]");
	ASSERT_NE(text, nullptr);
	EXPECT_EQ(text->text, std::wstring(L"a & b <c>")); // CDATA as text.

	// Duplicate name: the second label gets the #1 suffix.
	const Merge20FormElement* dupLabel = FindElement(result, L"label[name='title']#1");
	ASSERT_NE(dupLabel, nullptr);
	EXPECT_EQ(dupLabel->structPath, std::wstring(L"/form/label[1]"));
	EXPECT_EQ(dupLabel->text, std::wstring(L"dup"));

	const Merge20FormShape* labelShape = FindShape(result, L"label[name='title']");
	ASSERT_NE(labelShape, nullptr);
	EXPECT_DOUBLE_EQ(labelShape->x, 10.0);
	EXPECT_DOUBLE_EQ(labelShape->y, 20.0);
	EXPECT_DOUBLE_EQ(labelShape->x2, 210.0); // x + width.
	EXPECT_DOUBLE_EQ(labelShape->y2, 50.0); // y + height.
	EXPECT_DOUBLE_EQ(labelShape->point, 14.0);
	EXPECT_FALSE(labelShape->isLine);
	EXPECT_FALSE(labelShape->isEstimated);
	EXPECT_EQ(labelShape->name, std::wstring(L"title"));

	const Merge20FormShape* fieldShape = FindShape(result, L"field[name='amount']");
	ASSERT_NE(fieldShape, nullptr);
	EXPECT_DOUBLE_EQ(fieldShape->x2, 210.0);
	EXPECT_DOUBLE_EQ(fieldShape->y2, 90.0);
	EXPECT_DOUBLE_EQ(fieldShape->point, 12.0); // fontsize fallback.
	EXPECT_EQ(fieldShape->text, std::wstring(L"12345"));

	const Merge20FormShape* lineShape = FindShape(result, L"line[name='rule1']");
	ASSERT_NE(lineShape, nullptr);
	EXPECT_DOUBLE_EQ(lineShape->x, 0.0); // x1 fallback.
	EXPECT_DOUBLE_EQ(lineShape->y, 100.0); // y1 fallback.
	EXPECT_DOUBLE_EQ(lineShape->x2, 1000.0);
	EXPECT_DOUBLE_EQ(lineShape->y2, 100.0);
	EXPECT_TRUE(lineShape->isLine);
	EXPECT_FALSE(lineShape->isEstimated);

	const Merge20FormShape* boxShape = FindShape(result, L"box[height=40,left=5,top=120,width=300]");
	ASSERT_NE(boxShape, nullptr);
	EXPECT_DOUBLE_EQ(boxShape->x, 5.0); // left fallback.
	EXPECT_DOUBLE_EQ(boxShape->y, 120.0); // top fallback.
	EXPECT_DOUBLE_EQ(boxShape->x2, 305.0); // x + width.
	EXPECT_DOUBLE_EQ(boxShape->y2, 160.0); // y + height.
	EXPECT_DOUBLE_EQ(boxShape->point, 10.0); // Default point.
	EXPECT_EQ(boxShape->name, std::wstring(L"box")); // No name: tag.

	const Merge20FormShape* dupShape = FindShape(result, L"label[name='title']#1");
	ASSERT_NE(dupShape, nullptr);
	EXPECT_DOUBLE_EQ(dupShape->x2, 90.0); // Estimated: 10 + max(80, 5*10*1.5).
	EXPECT_DOUBLE_EQ(dupShape->y2, 224.0); // Estimated: 200 + max(24, 10*2).
	EXPECT_TRUE(dupShape->isEstimated);
}

TEST(Merge20FormXml, ParseEntitiesAndNamespaces)
{
	const Merge20FormParseResult result = Merge20ParseFormXml(kNamespacesXml);
	ASSERT_TRUE(result.ok);
	ASSERT_EQ(result.elements.size(), static_cast<size_t>(4));
	ASSERT_EQ(result.shapes.size(), static_cast<size_t>(2));

	// xmlns declarations are excluded from the signature key
	// (design section 3.3); the HTML key would list them.
	const Merge20FormElement* root = FindElement(result, L"fx:form[]");
	ASSERT_NE(root, nullptr);
	EXPECT_EQ(root->tag, std::wstring(L"fx:form"));
	EXPECT_EQ(root->attrs.size(), static_cast<size_t>(2));
	EXPECT_EQ(root->attrs.at(L"xmlns:fx"), std::wstring(L"http://example.com/fx"));
	EXPECT_EQ(root->attrs.at(L"xmlns"), std::wstring(L"http://example.com/d"));
	EXPECT_EQ(FindElement(result, L"fx:form[xmlns:fx=http://example.com/fx,xmlns=http://example.com/d]"), nullptr);

	const Merge20FormElement* item = FindElement(result, L"fx:item[name='a&b']");
	ASSERT_NE(item, nullptr);
	EXPECT_EQ(item->attrs.at(L"name"), std::wstring(L"a&b")); // &amp; decoded.
	EXPECT_EQ(item->text, std::wstring(L"x < y \"q\" AB")); // &lt; &quot; &#65; &#x42; decoded.
	EXPECT_EQ(item->structPath, std::wstring(L"/fx:form/fx:item[1]"));

	const Merge20FormShape* itemShape = FindShape(result, L"fx:item[name='a&b']");
	ASSERT_NE(itemShape, nullptr);
	EXPECT_DOUBLE_EQ(itemShape->x, 1.5);
	EXPECT_DOUBLE_EQ(itemShape->y, 2.5);
	EXPECT_DOUBLE_EQ(itemShape->x2, 81.5); // Estimated: 1.5 + max(80, 3*9*1.5).
	EXPECT_DOUBLE_EQ(itemShape->y2, 26.5); // Estimated: 2.5 + max(24, 9*2).
	EXPECT_DOUBLE_EQ(itemShape->point, 9.0);
	EXPECT_TRUE(itemShape->isEstimated);

	const Merge20FormShape* plainItem = FindShape(result, L"fx:item[x=3,y=4]");
	ASSERT_NE(plainItem, nullptr);
	EXPECT_EQ(plainItem->name, std::wstring(L"fx:item")); // No name: tag.
	EXPECT_DOUBLE_EQ(plainItem->x2, 83.0); // Estimated: 3 + max(80, 4*10*1.5).
	EXPECT_DOUBLE_EQ(plainItem->y2, 28.0); // Estimated: 4 + max(24, 10*2).
}

TEST(Merge20FormXml, ParseFailures)
{
	// Mismatched tags.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a><b></a></b>").ok);
	// Unclosed attribute quote.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a x=\"v></a>").ok);
	// Unclosed element.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a><b></a>").ok);
	EXPECT_FALSE(Merge20ParseFormXml(L"<a>").ok);
	// Bare '&' and '<' in text.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a>x & y</a>").ok);
	EXPECT_FALSE(Merge20ParseFormXml(L"<a>x < y</a>").ok);
	// No root element.
	EXPECT_FALSE(Merge20ParseFormXml(L"").ok);
	EXPECT_FALSE(Merge20ParseFormXml(L"hello").ok);
	// Trailing content after the root.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a/><b/>").ok);
	EXPECT_FALSE(Merge20ParseFormXml(L"<a/>junk").ok);
	// Unclosed comment / CDATA / processing instruction.
	EXPECT_FALSE(Merge20ParseFormXml(L"<!-- oops <a/>").ok);
	EXPECT_FALSE(Merge20ParseFormXml(L"<a><![CDATA[oops</a>").ok);
	EXPECT_FALSE(Merge20ParseFormXml(L"<?xml version=\"1.0\" <a/>").ok);
}

TEST(Merge20FormXml, UnknownEntityKeptAsIs)
{
	// Undefined entity: kept literally (design section 3.3). The
	// HTML DOMParser would fail this input; the fork does not.
	const Merge20FormParseResult kept = Merge20ParseFormXml(L"<a>x &foo; y</a>");
	ASSERT_TRUE(kept.ok);
	ASSERT_EQ(kept.elements.size(), static_cast<size_t>(1));
	EXPECT_EQ(kept.elements[0].text, std::wstring(L"x &foo; y"));

	// "&;" is not a reference.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a>&;</a>").ok);
	// Invalid character reference (NUL is not a valid XML char).
	EXPECT_FALSE(Merge20ParseFormXml(L"<a>&#0;</a>").ok);
	// A reference without its ';' is a bare '&'.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a>&amp</a>").ok);
	// Duplicate attribute.
	EXPECT_FALSE(Merge20ParseFormXml(L"<a x=\"1\" x=\"2\"/>").ok);
}

TEST(Merge20FormXml, ShapeEstimationAndLines)
{
	const Merge20FormParseResult result = Merge20ParseFormXml(kEstimationXml);
	ASSERT_TRUE(result.ok);
	ASSERT_EQ(result.elements.size(), static_cast<size_t>(5));
	ASSERT_EQ(result.shapes.size(), static_cast<size_t>(4));

	const Merge20FormShape* named = FindShape(result, L"field[name='ab']");
	ASSERT_NE(named, nullptr);
	EXPECT_DOUBLE_EQ(named->x, 50.0);
	EXPECT_DOUBLE_EQ(named->y, 70.0);
	EXPECT_DOUBLE_EQ(named->x2, 130.0); // Estimated: 50 + max(80, 2*10*1.5).
	EXPECT_DOUBLE_EQ(named->y2, 94.0); // Estimated: 70 + max(24, 10*2).
	EXPECT_TRUE(named->isEstimated);

	const Merge20FormShape* sized = FindShape(result, L"field[width=30,x=10,y=20]");
	ASSERT_NE(sized, nullptr);
	EXPECT_DOUBLE_EQ(sized->x2, 40.0); // x + width (not estimated).
	EXPECT_DOUBLE_EQ(sized->y2, 44.0); // Estimated.
	EXPECT_TRUE(sized->isEstimated);

	const Merge20FormShape* line = FindShape(result, L"myline[x=0,x2=5,y=0]");
	ASSERT_NE(line, nullptr);
	EXPECT_DOUBLE_EQ(line->x2, 5.0); // Given (not estimated).
	EXPECT_DOUBLE_EQ(line->y2, 24.0); // Estimated.
	EXPECT_TRUE(line->isLine);
	EXPECT_TRUE(line->isEstimated);

	// Tag case does not matter for the line check (HTML lower-cases).
	const Merge20FormShape* upper = FindShape(result, L"UPPERLINE[x=1,x2=2,y=1,y2=2]");
	ASSERT_NE(upper, nullptr);
	EXPECT_TRUE(upper->isLine);
	EXPECT_FALSE(upper->isEstimated);
}

TEST(Merge20FormXml, ShapeFontName)
{
	// F3 (design section 6 (6)): the font name of a shape comes
	// from the `fontname` / `font` / `fontName` attribute when
	// present, and stays empty otherwise (the canvas then draws
	// with the default font and shows the approximation note).
	const wchar_t* xml =
		L"<form>\n"
		L"  <label name=\"a\" x=\"10\" y=\"20\" width=\"100\" height=\"30\" fontname=\"MS Gothic\" />\n"
		L"  <label name=\"b\" x=\"10\" y=\"60\" width=\"100\" height=\"30\" font=\"Arial\" />\n"
		L"  <label name=\"c\" x=\"10\" y=\"100\" width=\"100\" height=\"30\" />\n"
		L"</form>\n";
	const Merge20FormParseResult result = Merge20ParseFormXml(xml);
	ASSERT_TRUE(result.ok);
	ASSERT_EQ(result.shapes.size(), static_cast<size_t>(3));
	const Merge20FormShape* a = FindShape(result, L"label[name='a']");
	ASSERT_NE(a, nullptr);
	EXPECT_EQ(a->fontName, L"MS Gothic");
	const Merge20FormShape* b = FindShape(result, L"label[name='b']");
	ASSERT_NE(b, nullptr);
	EXPECT_EQ(b->fontName, L"Arial");
	const Merge20FormShape* c = FindShape(result, L"label[name='c']");
	ASSERT_NE(c, nullptr);
	EXPECT_TRUE(c->fontName.empty());
}

TEST(Merge20FormXml, DuplicateKeysUseGlobalCounter)
{
	// HTML ground truth: the counter is global, so the second `i`
	// gets #2 (the second `g` took #1).
	const Merge20FormParseResult result = Merge20ParseFormXml(kDuplicateNamesXml);
	ASSERT_TRUE(result.ok);
	ASSERT_EQ(result.elements.size(), static_cast<size_t>(5));
	ASSERT_EQ(result.shapes.size(), static_cast<size_t>(2));

	EXPECT_NE(FindElement(result, L"r[]"), nullptr);
	EXPECT_NE(FindElement(result, L"g[name='n']"), nullptr);
	EXPECT_NE(FindElement(result, L"i[name='x']"), nullptr);
	EXPECT_NE(FindElement(result, L"g[name='n']#1"), nullptr);
	EXPECT_NE(FindElement(result, L"i[name='x']#2"), nullptr);
	EXPECT_EQ(FindElement(result, L"i[name='x']#1"), nullptr);

	const Merge20FormShape* first = FindShape(result, L"i[name='x']");
	ASSERT_NE(first, nullptr);
	EXPECT_DOUBLE_EQ(first->x, 1.0);
	EXPECT_DOUBLE_EQ(first->x2, 81.0);
	EXPECT_DOUBLE_EQ(first->y2, 25.0);
	const Merge20FormShape* second = FindShape(result, L"i[name='x']#2");
	ASSERT_NE(second, nullptr);
	EXPECT_DOUBLE_EQ(second->x, 2.0);
	EXPECT_DOUBLE_EQ(second->x2, 82.0);
	EXPECT_DOUBLE_EQ(second->y2, 26.0);
}

TEST(Merge20FormXml, StructPaths)
{
	const Merge20FormParseResult result = Merge20ParseFormXml(kDuplicateNamesXml);
	ASSERT_TRUE(result.ok);
	const Merge20FormElement* root = FindElement(result, L"r[]");
	ASSERT_NE(root, nullptr);
	EXPECT_EQ(root->structPath, std::wstring(L"/r"));
	const Merge20FormElement* secondG = FindElement(result, L"g[name='n']#1");
	ASSERT_NE(secondG, nullptr);
	EXPECT_EQ(secondG->structPath, std::wstring(L"/r/g[2]"));
	const Merge20FormElement* secondI = FindElement(result, L"i[name='x']#2");
	ASSERT_NE(secondI, nullptr);
	EXPECT_EQ(secondI->structPath, std::wstring(L"/r/g[2]/i[1]"));

	const Merge20FormParseResult basic = Merge20ParseFormXml(kFormBasicXml);
	ASSERT_TRUE(basic.ok);
	const Merge20FormElement* page = FindElement(basic, L"page[name='page1']");
	ASSERT_NE(page, nullptr);
	EXPECT_EQ(page->structPath, std::wstring(L"/form/page[1]"));
}

TEST(Merge20FormXml, NonNumericCoordsMakeNoShape)
{
	const Merge20FormParseResult result = Merge20ParseFormXml(
		L"<r><a x=\"abc\" y=\"1\"/><b x=\"1\"/><c x=\"1\" y=\"2\"/></r>");
	ASSERT_TRUE(result.ok);
	ASSERT_EQ(result.elements.size(), static_cast<size_t>(4));
	ASSERT_EQ(result.shapes.size(), static_cast<size_t>(1));
	EXPECT_NE(FindShape(result, L"c[x=1,y=2]"), nullptr);
}

TEST(Merge20FormXml, ParseNumber)
{
	double value = 0.0;
	EXPECT_TRUE(Merge20FormParseNumber(L"12.5", value));
	EXPECT_DOUBLE_EQ(value, 12.5);
	EXPECT_TRUE(Merge20FormParseNumber(L"1e3", value));
	EXPECT_DOUBLE_EQ(value, 1000.0);
	EXPECT_TRUE(Merge20FormParseNumber(L"", value));
	EXPECT_DOUBLE_EQ(value, 0.0); // JavaScript Number("") is 0.
	EXPECT_TRUE(Merge20FormParseNumber(L"  ", value));
	EXPECT_DOUBLE_EQ(value, 0.0);
	EXPECT_TRUE(Merge20FormParseNumber(L".5", value));
	EXPECT_DOUBLE_EQ(value, 0.5);
	EXPECT_TRUE(Merge20FormParseNumber(L"-3", value));
	EXPECT_DOUBLE_EQ(value, -3.0);
	EXPECT_TRUE(Merge20FormParseNumber(L"0x10", value));
	EXPECT_DOUBLE_EQ(value, 16.0);
	EXPECT_FALSE(Merge20FormParseNumber(L"12px", value));
	EXPECT_FALSE(Merge20FormParseNumber(L"abc", value));
	EXPECT_FALSE(Merge20FormParseNumber(L"1.2.3", value));
	EXPECT_TRUE(Merge20FormParseNumber(L"NaN", value));
	EXPECT_TRUE(std::isnan(value));
}

