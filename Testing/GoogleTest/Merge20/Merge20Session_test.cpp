// Merge20 session save/restore (S8): unit tests for the pure module
// Merge20Session (UTF-8 codec, JSON read/write, session conversion,
// file I/O, change detection). MFC-free; runs in UnitTests.
#include "pch.h"
#include "Merge20/Merge20Session.h"

// This file is ASCII; Japanese text is written with \u escapes.
namespace
{

	// Decode a UTF-8 literal into a String.
	String FromUtf8(const std::string& utf8)
	{
		String out;
		EXPECT_TRUE(Merge20Utf8Decode(utf8, out));
		return out;
	}

	Merge20Session MakeSession()
	{
		Merge20Session session;
		session.savedAt = L"2026-11-25 23:41:00";
		session.leftPath = L"C:\\work\\OLD";
		session.rightPath = L"C:\\work\\NEW";
		session.compareOptions.recursive = true;
		session.compareOptions.fileFilter = L"*.cpp";
		session.compareOptions.compareMethod = 0;
		session.compareOptions.ignoreWhitespace = 1;
		session.compareOptions.ignoreCase = true;
		session.display.showIdentical = false;
		session.display.treeMode = true;
		session.display.reviewFilter[1] = false;
		session.display.columnOrders = L"0,1,2";
		session.display.columnWidths = L"100,200";
		session.display.hiddenItems = { L"src\\.git", L"bin" };
		session.display.focusedPath = L"src\\foo.cpp";
		session.display.topIndex = 3;
		{
			Merge20SessionFile f;
			f.relPath = L"src\\foo.cpp";
			f.review = ReviewState::DONE;
			f.includeInOutput = true;
			f.classified = true;
			f.action = ActionCategory::MERGE_CLEAN_OLD;
			f.risk = RiskLevel::MEDIUM;
			f.nMissing = 3;
			f.nAdded = 1;
			f.nExcluded = 2;
			f.summary = FromUtf8("\xe6\xac\xa0\xe8\x90\xbd\xef\xbc\x9a" "3" "\xe8\xa1\x8c");
			f.memo = FromUtf8("\xe7\xa2\xba\xe8\xaa\x8d\xe6\xb8\x88\xe3\x81\xbf\xe3\x80\x82\"\xe5\xbc\x95\xe7\x94\xa8\xe7\xac\xa6\"");
			f.leftSize = 1234;
			f.leftMtime = 1700000000000000LL;
			f.rightSize = 1250;
			f.rightMtime = 1700000001000000LL;
			f.nsdiffs = 4;
			f.nidiffs = 0;
			Merge20DiffJudgment j;
			j.diffIdx = 0;
			j.op = 4;
			j.begin[0] = 10;
			j.end[0] = 12;
			j.begin[1] = 12;
			j.end[1] = 14;
			j.state = 2;
			j.srcPanes = { 1 };
			f.diffs.push_back(j);
			Merge20DiffJudgment j2;
			j2.diffIdx = 1;
			j2.op = 1;
			j2.begin[0] = 20;
			j2.end[0] = 20;
			j2.state = 3; // Unresolved, no sources
			f.diffs.push_back(j2);
			session.files.push_back(f);
		}
		{
			Merge20SessionFile f;
			f.relPath = L"only-old.txt";
			f.review = ReviewState::TODO;
			f.includeInOutput = true;
			f.leftSize = 55;
			f.leftMtime = 1700000000000000LL;
			f.rightSize = -1;
			f.rightMtime = -1;
			session.files.push_back(f);
		}
		return session;
	}

	Merge20FileIdentity IdentityOf(const Merge20SessionFile& saved)
	{
		Merge20FileIdentity identity;
		identity.leftSize = saved.leftSize;
		identity.leftMtime = saved.leftMtime;
		identity.rightSize = saved.rightSize;
		identity.rightMtime = saved.rightMtime;
		identity.thirdSize = saved.thirdSize;
		identity.thirdMtime = saved.thirdMtime;
		identity.nsdiffs = saved.nsdiffs;
		identity.nidiffs = saved.nidiffs;
		return identity;
	}

} // namespace

TEST(Merge20Session, Utf8RoundTrip)
{
	String text = FromUtf8("\xe7\xa8\x8e\xe6\x95\xb4\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
	EXPECT_EQ(Merge20Utf8Encode(text), "\xe7\xa8\x8e\xe6\x95\xb4\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
	String decoded;
	EXPECT_TRUE(Merge20Utf8Decode(Merge20Utf8Encode(text), decoded));
	EXPECT_EQ(decoded, text);
	// Emoji (astral plane).
	String emoji = FromUtf8("\xf0\x9f\x98\x80");
	EXPECT_EQ(Merge20Utf8Encode(emoji), "\xf0\x9f\x98\x80");
	// Invalid bytes become U+FFFD, decoding continues.
	String out;
	EXPECT_TRUE(Merge20Utf8Decode(std::string("a\xff\xfe" "b"), out));
	ASSERT_EQ(out.size(), 4u);
	EXPECT_EQ(out[0], L'a');
	EXPECT_EQ(out[3], L'b');
	EXPECT_EQ(out[1], static_cast<wchar_t>(0xFFFD));
	EXPECT_EQ(out[2], static_cast<wchar_t>(0xFFFD));
}

TEST(Merge20Session, JsonRoundTrip)
{
	Merge20JsonValue root;
	std::string err;
	ASSERT_TRUE(Merge20JsonParse("{\"a\": [1, true, null, \"x\\n\\u3042\"], \"b\": {}}", root, err));
	EXPECT_TRUE(err.empty());
	ASSERT_NE(root.Find(L"a"), nullptr);
	ASSERT_EQ(root.Find(L"a")->array.size(), 4u);
	EXPECT_EQ(root.Find(L"a")->array[0].AsInt(-1), 1);
	EXPECT_TRUE(root.Find(L"a")->array[1].AsBool(false));
	EXPECT_EQ(root.Find(L"a")->array[2].type, Merge20JsonValue::Type::Null);
	EXPECT_EQ(root.Find(L"a")->array[3].text, FromUtf8("x\n\xe3\x81\x82"));
	EXPECT_EQ(root.Find(L"missing"), nullptr);
	std::string out = Merge20JsonStringify(root);
	Merge20JsonValue root2;
	ASSERT_TRUE(Merge20JsonParse(out, root2, err));
	EXPECT_EQ(root2.Find(L"a")->array[3].text, FromUtf8("x\n\xe3\x81\x82"));
}

TEST(Merge20Session, JsonErrors)
{
	Merge20JsonValue v;
	std::string err;
	EXPECT_FALSE(Merge20JsonParse("{\"a\":1} x", v, err));
	EXPECT_FALSE(err.empty());
	EXPECT_FALSE(Merge20JsonParse("\"abc", v, err));
	EXPECT_FALSE(Merge20JsonParse("[1.5]", v, err));
	EXPECT_FALSE(Merge20JsonParse("[1,]", v, err));
	EXPECT_FALSE(Merge20JsonParse("\"\\q\"", v, err));
	EXPECT_FALSE(Merge20JsonParse("", v, err));
	EXPECT_FALSE(Merge20JsonParse("{\"a\":}", v, err));
	// BOM is tolerated.
	ASSERT_TRUE(Merge20JsonParse("\xef\xbb\xbf{\"a\":1}", v, err));
	EXPECT_EQ(v.Find(L"a")->AsInt(0), 1);
	// Surrogate-pair escape decodes to one astral character.
	ASSERT_TRUE(Merge20JsonParse("\"\\ud83d\\ude00\"", v, err));
	EXPECT_EQ(Merge20Utf8Encode(v.text), "\xf0\x9f\x98\x80");
	// A lone surrogate becomes U+FFFD.
	ASSERT_TRUE(Merge20JsonParse("\"\\ud83d\"", v, err));
	EXPECT_EQ(Merge20Utf8Encode(v.text), "\xef\xbf\xbd");
}

TEST(Merge20Session, SessionRoundTrip)
{
	Merge20Session session = MakeSession();
	std::string json;
	ASSERT_TRUE(Merge20SessionToJson(session, json));
	// Japanese text is stored as raw UTF-8 (readable in the file).
	EXPECT_NE(json.find("\xe7\xa2\xba\xe8\xaa\x8d"), std::string::npos);
	EXPECT_NE(json.find("\"app\": \"Merge20\""), std::string::npos);
	EXPECT_NE(json.find("\"version\": 1"), std::string::npos);

	Merge20Session back;
	std::string err;
	int skipped = -1;
	ASSERT_TRUE(Merge20SessionFromJson(json, back, err, skipped));
	EXPECT_TRUE(err.empty());
	EXPECT_EQ(skipped, 0);
	EXPECT_EQ(back.savedAt, session.savedAt);
	EXPECT_EQ(back.leftPath, session.leftPath);
	EXPECT_EQ(back.rightPath, session.rightPath);
	EXPECT_TRUE(back.thirdPath.empty());
	EXPECT_EQ(back.compareOptions.fileFilter, L"*.cpp");
	EXPECT_EQ(back.compareOptions.ignoreWhitespace, 1);
	EXPECT_TRUE(back.compareOptions.ignoreCase);
	EXPECT_FALSE(back.display.showIdentical);
	EXPECT_TRUE(back.display.treeMode);
	EXPECT_FALSE(back.display.reviewFilter[1]);
	EXPECT_EQ(back.display.columnOrders, L"0,1,2");
	ASSERT_EQ(back.display.hiddenItems.size(), 2u);
	EXPECT_EQ(back.display.focusedPath, L"src\\foo.cpp");
	EXPECT_EQ(back.display.topIndex, 3);
	ASSERT_EQ(back.files.size(), 2u);
	const Merge20SessionFile& f = back.files[0];
	EXPECT_EQ(f.relPath, L"src\\foo.cpp");
	EXPECT_EQ(f.review, ReviewState::DONE);
	EXPECT_TRUE(f.classified);
	EXPECT_EQ(f.action, ActionCategory::MERGE_CLEAN_OLD);
	EXPECT_EQ(f.risk, RiskLevel::MEDIUM);
	EXPECT_EQ(f.nMissing, 3);
	EXPECT_EQ(f.nAdded, 1);
	EXPECT_EQ(f.nExcluded, 2);
	EXPECT_EQ(f.summary, session.files[0].summary);
	EXPECT_EQ(f.memo, session.files[0].memo);
	EXPECT_EQ(f.leftSize, 1234);
	EXPECT_EQ(f.leftMtime, 1700000000000000LL);
	EXPECT_EQ(f.nsdiffs, 4);
	EXPECT_EQ(f.nidiffs, 0);
	ASSERT_EQ(f.diffs.size(), 2u);
	EXPECT_EQ(f.diffs[0].diffIdx, 0);
	EXPECT_EQ(f.diffs[0].op, 4);
	EXPECT_EQ(f.diffs[0].state, 2);
	EXPECT_EQ(f.diffs[0].begin[0], 10);
	EXPECT_EQ(f.diffs[0].end[1], 14);
	ASSERT_EQ(f.diffs[0].srcPanes.size(), 1u);
	EXPECT_EQ(f.diffs[0].srcPanes[0], 1);
	EXPECT_EQ(f.diffs[1].diffIdx, 1);
	EXPECT_EQ(f.diffs[1].state, 3);
	EXPECT_TRUE(f.diffs[1].srcPanes.empty());
	EXPECT_EQ(back.files[1].rightSize, -1);
	EXPECT_EQ(back.files[1].rightMtime, -1);
}

TEST(Merge20Session, SessionFormatErrors)
{
	Merge20Session back;
	std::string err;
	int skipped = 0;
	EXPECT_FALSE(Merge20SessionFromJson("{\"app\":\"Other\",\"version\":1}", back, err, skipped));
	EXPECT_FALSE(err.empty());
	EXPECT_FALSE(Merge20SessionFromJson("{\"app\":\"Merge20\",\"version\":2}", back, err, skipped));
	EXPECT_FALSE(Merge20SessionFromJson("{\"app\":\"Merge20\"}", back, err, skipped));
	EXPECT_FALSE(Merge20SessionFromJson("not json", back, err, skipped));
	// One bad file entry is skipped, the good one survives.
	ASSERT_TRUE(Merge20SessionFromJson(
		"{\"app\":\"Merge20\",\"version\":1,\"files\":["
		"{\"path\":\"good.txt\",\"review\":1},"
		"{\"path\":123},"
		"{\"path\":\"alsogood.txt\",\"review\":9}]}",
		back, err, skipped));
	EXPECT_EQ(skipped, 2);
	ASSERT_EQ(back.files.size(), 1u);
	EXPECT_EQ(back.files[0].relPath, L"good.txt");
	EXPECT_EQ(back.files[0].review, ReviewState::DONE);
	// A bad diff inside a good file is dropped, the file survives.
	ASSERT_TRUE(Merge20SessionFromJson(
		"{\"app\":\"Merge20\",\"version\":1,\"files\":["
		"{\"path\":\"f.txt\",\"diffs\":["
		"{\"diffIdx\":0,\"op\":4,\"state\":2,\"srcPanes\":[1]},"
		"{\"diffIdx\":-1,\"op\":4,\"state\":2}]}]}",
		back, err, skipped));
	ASSERT_EQ(back.files.size(), 1u);
	ASSERT_EQ(back.files[0].diffs.size(), 1u);
	EXPECT_EQ(back.files[0].diffs[0].diffIdx, 0);
}

TEST(Merge20Session, FileChanged)
{
	Merge20Session session = MakeSession();
	const Merge20SessionFile& saved = session.files[0];
	Merge20FileIdentity current = IdentityOf(saved);
	EXPECT_FALSE(Merge20SessionFileChanged(saved, current));
	current.rightSize = 1251;
	EXPECT_TRUE(Merge20SessionFileChanged(saved, current));
	current.rightSize = 1250;
	current.leftMtime = 1700000000000001LL;
	EXPECT_TRUE(Merge20SessionFileChanged(saved, current));
	current.leftMtime = 1700000000000000LL;
	current.nsdiffs = 5;
	EXPECT_TRUE(Merge20SessionFileChanged(saved, current));
	current.nsdiffs = 4;
	current.nidiffs = 1;
	EXPECT_TRUE(Merge20SessionFileChanged(saved, current));
	current.nidiffs = 0;
	// Unknown counts never count as a change.
	current.nsdiffs = -1;
	current.nidiffs = -1;
	EXPECT_FALSE(Merge20SessionFileChanged(saved, current));
	// A missing side stays missing.
	const Merge20SessionFile& saved2 = session.files[1];
	Merge20FileIdentity current2 = IdentityOf(saved2);
	EXPECT_FALSE(Merge20SessionFileChanged(saved2, current2));
	current2.rightSize = 10;
	EXPECT_TRUE(Merge20SessionFileChanged(saved2, current2));
}
