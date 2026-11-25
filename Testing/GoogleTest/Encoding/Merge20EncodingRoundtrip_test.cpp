/**
 * @file Merge20EncodingRoundtrip_test.cpp
 *
 * @brief Merge20 (S0) roundtrip tests for Shift_JIS / EUC-JP / UTF-8.
 *
 * Design section 4.3: the EUC-JP file that was once written out as UTF-8
 * in the HTML tool must never come back. These tests guard the two halves
 * of the roundtrip at the level UnitTests can reach (CDiffTextBuffer is
 * not linked into UnitTests, so the tests use UniFile directly):
 * (a) codepage_detect::Guess tells the encodings apart, and
 * (b) UniStdioFile writes bytes in the requested codepage and UniMemFile
 * reads them back to the same text, i.e. write + read is byte-identical.
 *
 * The Shift_JIS / EUC-JP discrimination in (a) goes through MLang
 * (ExConverter::detectInputCodepage, autodetect type 50932, the same
 * default the application uses for OPT_CP_DETECT). The shipped fixtures
 * under Testing/Data/Codepages/ are 6 KB of real Japanese text, which is
 * what MLang's Japanese auto-detection is designed for.
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes and the expected byte sequences are explicit byte arrays.
 */
#include "pch.h"
#include <cstring>
#include <gtest/gtest.h>
#include "codepage_detect.h"
#include "charsets.h"
#include "UniFile.h"
#include "TempFile.h"

namespace
{

	// Test text: Japanese (every character exists in Shift_JIS, EUC-JP
	// and UTF-8) plus ASCII digits, CRLF line endings.
	const wchar_t kText[] = L"\u56FA\u5B9A\u8CC7\u7523\u7A0E\u306E\u8A08\u7B97\u3067\u3059\u3002\r\n\u7AEF\u6570\u51E6\u7406\u3092\u5909\u66F4\u3057\u307E\u3057\u305F\u3002\r\n12345\r\n";

	// kText encoded as Shift_JIS (codepage 932), 57 bytes.
	static const unsigned char kShiftJIS[] = {
		0x8C, 0xC5, 0x92, 0xE8, 0x8E, 0x91, 0x8E, 0x59, 0x90, 0xC5, 0x82, 0xCC, 0x8C, 0x76, 0x8E, 0x5A,
		0x82, 0xC5, 0x82, 0xB7, 0x81, 0x42, 0x0D, 0x0A, 0x92, 0x5B, 0x90, 0x94, 0x8F, 0x88, 0x97, 0x9D,
		0x82, 0xF0, 0x95, 0xCF, 0x8D, 0x58, 0x82, 0xB5, 0x82, 0xDC, 0x82, 0xB5, 0x82, 0xBD, 0x81, 0x42,
		0x0D, 0x0A, 0x31, 0x32, 0x33, 0x34, 0x35, 0x0D, 0x0A,
	};

	// kText encoded as EUC-JP (codepage 51932), 57 bytes.
	static const unsigned char kEucJp[] = {
		0xB8, 0xC7, 0xC4, 0xEA, 0xBB, 0xF1, 0xBB, 0xBA, 0xC0, 0xC7, 0xA4, 0xCE, 0xB7, 0xD7, 0xBB, 0xBB,
		0xA4, 0xC7, 0xA4, 0xB9, 0xA1, 0xA3, 0x0D, 0x0A, 0xC3, 0xBC, 0xBF, 0xF4, 0xBD, 0xE8, 0xCD, 0xFD,
		0xA4, 0xF2, 0xCA, 0xD1, 0xB9, 0xB9, 0xA4, 0xB7, 0xA4, 0xDE, 0xA4, 0xB7, 0xA4, 0xBF, 0xA1, 0xA3,
		0x0D, 0x0A, 0x31, 0x32, 0x33, 0x34, 0x35, 0x0D, 0x0A,
	};

	// kText encoded as UTF-8 (codepage 65001, no BOM), 80 bytes.
	static const unsigned char kUtf8[] = {
		0xE5, 0x9B, 0xBA, 0xE5, 0xAE, 0x9A, 0xE8, 0xB3, 0x87, 0xE7, 0x94, 0xA3, 0xE7, 0xA8, 0x8E, 0xE3,
		0x81, 0xAE, 0xE8, 0xA8, 0x88, 0xE7, 0xAE, 0x97, 0xE3, 0x81, 0xA7, 0xE3, 0x81, 0x99, 0xE3, 0x80,
		0x82, 0x0D, 0x0A, 0xE7, 0xAB, 0xAF, 0xE6, 0x95, 0xB0, 0xE5, 0x87, 0xA6, 0xE7, 0x90, 0x86, 0xE3,
		0x82, 0x92, 0xE5, 0xA4, 0x89, 0xE6, 0x9B, 0xB4, 0xE3, 0x81, 0x97, 0xE3, 0x81, 0xBE, 0xE3, 0x81,
		0x97, 0xE3, 0x81, 0x9F, 0xE3, 0x80, 0x82, 0x0D, 0x0A, 0x31, 0x32, 0x33, 0x34, 0x35, 0x0D, 0x0A,
	};

	class Merge20EncodingRoundtripTest : public testing::Test
	{
	protected:
		Merge20EncodingRoundtripTest()
		{
			charsets_init();
		}

		virtual ~Merge20EncodingRoundtripTest()
		{
			charsets_cleanup();
		}
	};

	static TempFile WriteEncodedFile(int codepage, const String& text, bool bom)
	{
		TempFile tmpfile;
		tmpfile.Create();
		UniStdioFile file;
		if (file.OpenCreate(tmpfile.GetPath()))
		{
			file.SetCodepage(codepage);
			if (bom)
			{
				file.SetBom(true);
				file.WriteBom();
			}
			file.WriteString(text);
			file.Close();
		}
		return tmpfile;
	}

	static void ExpectFileBytes(const TempFile& tmpfile, const unsigned char* expected, size_t expectedSize)
	{
		UniMemFile file;
		ASSERT_TRUE(file.OpenReadOnly(tmpfile.GetPath()));
		ASSERT_EQ(static_cast<int64_t>(expectedSize), file.GetFileSize());
		ASSERT_NE(nullptr, file.GetBase());
		EXPECT_EQ(0, memcmp(file.GetBase(), expected, expectedSize));
	}

	static String ReadFileAsUnicode(const TempFile& tmpfile, int codepage, bool* pLossy)
	{
		UniMemFile file;
		String text;
		*pLossy = false;
		if (file.OpenReadOnly(tmpfile.GetPath()))
		{
			// Skip a BOM if present, as the application loader does.
			file.ReadBom();
			file.SetCodepage(codepage);
			*pLossy = file.ReadStringAll(text);
		}
		return text;
	}

	TEST_F(Merge20EncodingRoundtripTest, GuessUtf8Bom)
	{
		const unsigned char bomUtf8[] = { 0xEF, 0xBB, 0xBF, 'a', 'b', 'c' };
		FileTextEncoding enc = codepage_detect::Guess(_T(".txt"), bomUtf8, sizeof bomUtf8, 0);
		EXPECT_EQ(65001, enc.m_codepage);
		EXPECT_EQ(true, enc.m_bom);
		EXPECT_EQ(ucr::UTF8, enc.m_unicoding);
	}

	TEST_F(Merge20EncodingRoundtripTest, GuessUtf8NoBom)
	{
		// Valid UTF-8 without a BOM is detected as UTF-8 when guessing is on.
		FileTextEncoding enc = codepage_detect::Guess(_T(".txt"), kUtf8, sizeof kUtf8, 1);
		EXPECT_EQ(65001, enc.m_codepage);
		EXPECT_EQ(false, enc.m_bom);
		EXPECT_EQ(ucr::UTF8, enc.m_unicoding);
	}

	TEST_F(Merge20EncodingRoundtripTest, GuessAscii)
	{
		const char ascii[] = "hello\r\n";
		FileTextEncoding enc = codepage_detect::Guess(_T(".txt"), ascii, sizeof ascii - 1, 0);
		EXPECT_EQ(ucr::getDefaultCodepage(), enc.m_codepage);
		EXPECT_EQ(false, enc.m_bom);
		EXPECT_EQ(ucr::NONE, enc.m_unicoding);
	}

	TEST_F(Merge20EncodingRoundtripTest, GuessUtf16LeBom)
	{
		const unsigned char bomUcs2Le[] = { 0xFF, 0xFE, 'a', 0x00 };
		FileTextEncoding enc = codepage_detect::Guess(_T(".txt"), bomUcs2Le, sizeof bomUcs2Le, 0);
		EXPECT_EQ(1200, enc.m_codepage);
		EXPECT_EQ(true, enc.m_bom);
		EXPECT_EQ(ucr::UCS2LE, enc.m_unicoding);
	}

	TEST_F(Merge20EncodingRoundtripTest, GuessJapaneseFixtures)
	{
		// Japanese auto-detection (MLang, autodetect type 50932, the same
		// default the application uses for OPT_CP_DETECT) must tell the
		// shipped Shift_JIS and EUC-JP fixtures apart.
		const int guessJapanese = static_cast<int>((50932u << 16) | 3u);
		FileTextEncoding enc;
		enc = codepage_detect::Guess(_T("../../Data/Codepages/CP932/DiffItem.h"), guessJapanese);
		EXPECT_EQ(932, enc.m_codepage);
		EXPECT_EQ(false, enc.m_bom);
		EXPECT_EQ(ucr::NONE, enc.m_unicoding);
		enc = codepage_detect::Guess(_T("../../Data/Codepages/CP51932/DiffItem.h"), guessJapanese);
		EXPECT_EQ(51932, enc.m_codepage);
		EXPECT_EQ(false, enc.m_bom);
		EXPECT_EQ(ucr::NONE, enc.m_unicoding);
	}

	TEST_F(Merge20EncodingRoundtripTest, RoundtripShiftJIS)
	{
		TempFile tmpfile = WriteEncodedFile(932, kText, false);
		ExpectFileBytes(tmpfile, kShiftJIS, sizeof kShiftJIS);
		bool lossy = false;
		EXPECT_EQ(String(kText), ReadFileAsUnicode(tmpfile, 932, &lossy));
		EXPECT_FALSE(lossy);
	}

	TEST_F(Merge20EncodingRoundtripTest, RoundtripEucJp)
	{
		// The regression to prevent: an EUC-JP file written out as UTF-8.
		TempFile tmpfile = WriteEncodedFile(51932, kText, false);
		ExpectFileBytes(tmpfile, kEucJp, sizeof kEucJp);
		bool lossy = false;
		EXPECT_EQ(String(kText), ReadFileAsUnicode(tmpfile, 51932, &lossy));
		EXPECT_FALSE(lossy);
	}

	TEST_F(Merge20EncodingRoundtripTest, RoundtripUtf8)
	{
		TempFile tmpfile = WriteEncodedFile(65001, kText, false);
		ExpectFileBytes(tmpfile, kUtf8, sizeof kUtf8);
		bool lossy = false;
		EXPECT_EQ(String(kText), ReadFileAsUnicode(tmpfile, 65001, &lossy));
		EXPECT_FALSE(lossy);
		// The written file must also be detected as UTF-8 without a BOM.
		FileTextEncoding enc = codepage_detect::Guess(tmpfile.GetPath(), 1);
		EXPECT_EQ(65001, enc.m_codepage);
		EXPECT_EQ(false, enc.m_bom);
		EXPECT_EQ(ucr::UTF8, enc.m_unicoding);
	}

	TEST_F(Merge20EncodingRoundtripTest, RoundtripUtf8Bom)
	{
		TempFile tmpfile = WriteEncodedFile(65001, kText, true);
		unsigned char expected[3 + sizeof kUtf8];
		expected[0] = 0xEF;
		expected[1] = 0xBB;
		expected[2] = 0xBF;
		memcpy(expected + 3, kUtf8, sizeof kUtf8);
		ExpectFileBytes(tmpfile, expected, sizeof expected);
		bool lossy = false;
		EXPECT_EQ(String(kText), ReadFileAsUnicode(tmpfile, 65001, &lossy));
		EXPECT_FALSE(lossy);
		// The written file must be detected as UTF-8 with a BOM.
		FileTextEncoding enc = codepage_detect::Guess(tmpfile.GetPath(), 0);
		EXPECT_EQ(65001, enc.m_codepage);
		EXPECT_EQ(true, enc.m_bom);
		EXPECT_EQ(ucr::UTF8, enc.m_unicoding);
	}

}  // namespace
