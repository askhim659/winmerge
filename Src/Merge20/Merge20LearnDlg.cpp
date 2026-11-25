// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20LearnDlg.cpp
 *
 * @brief Implementation of the Merge20 learning dialog (Phase 3 L4).
 */
#include "StdAfx.h"
#include "Merge20/Merge20LearnDlg.h"
#include "Merge20/Merge20Learn.h"
#include "Merge20/Merge20LearnDoc.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include <algorithm>

IMPLEMENT_DYNAMIC(Merge20LearnDlg, CTrDialog)

namespace
{
	/** @brief Rules shown in the dialog (HTML `learnList` shows the top 50). */
	constexpr size_t kMaxRulesShown = 50;
	/** @brief Exact keys are long; the HTML tool shows key[6..90) (84 chars). */
	constexpr size_t kMaxExactKeyChars = 84;
}

Merge20LearnDlg::Merge20LearnDlg(CWnd* pParent)
	: CTrDialog(Merge20LearnDlg::IDD, pParent)
{
}

void Merge20LearnDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MERGE20_LEARN_SOURCES, m_sourcesList);
	DDX_Control(pDX, IDC_MERGE20_LEARN_RULES, m_rulesList);
}

BEGIN_MESSAGE_MAP(Merge20LearnDlg, CTrDialog)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_TOGGLE, OnToggle)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_DELETE_SOURCE, OnDeleteSource)
	ON_LBN_SELCHANGE(IDC_MERGE20_LEARN_SOURCES, OnSelChangeSources)
END_MESSAGE_MAP()

BOOL Merge20LearnDlg::OnInitDialog()
{
	__super::OnInitDialog();
	// L5 wires these buttons (design section 5: export / import /
	// merged export / clear are L5's scope). They are placed here so
	// the dialog already has the HTML modal's shape, but stay
	// disabled until L5 so they cannot run an unwired action.
	GetDlgItem(IDC_MERGE20_LEARN_EXPORT)->EnableWindow(FALSE);
	GetDlgItem(IDC_MERGE20_LEARN_IMPORT)->EnableWindow(FALSE);
	GetDlgItem(IDC_MERGE20_LEARN_MERGE_SOURCES)->EnableWindow(FALSE);
	GetDlgItem(IDC_MERGE20_LEARN_CLEAR)->EnableWindow(FALSE);
	RefreshAll();
	return TRUE;
}

void Merge20LearnDlg::RefreshAll()
{
	Merge20LearnStore& store = Merge20LearnGetStore();

	// ON/OFF state (same key as the compare options page, L1).
	const bool enabled = Merge20LearnIsEnabled();
	SetDlgItemText(IDC_MERGE20_LEARN_STATE,
		(enabled ?
			_("Current state: ON (suggestions and recording active)") :
			_("Current state: OFF (no suggestions, no recording)")).c_str());

	// Statistics: patterns / judgments / sources / candidates shown.
	// The HTML tool's "auto-judged in this analysis: N" is dropped:
	// there is no automatic application in the fork (spec section
	// 4(c)), so the live candidate count takes its place (design
	// section 3.3).
	const String patterns = strutils::to_str(static_cast<int>(store.RuleCount()));
	const String judgments = strutils::to_str(store.TotalJudgments());
	const String sources = strutils::to_str(static_cast<int>(store.GetSources().size()));
	const String candidates = strutils::to_str(Merge20LearnCountOpenCandidates());
	const String* statArgs[] = { &patterns, &judgments, &sources, &candidates };
	SetDlgItemText(IDC_MERGE20_LEARN_STATS,
		strutils::format_strings(_("Patterns: %1 / Judgments: %2 / Sources: %3 / Suggestions shown: %4"), statArgs, 4).c_str());

	// A broken learning file never blocks startup (L1); the failure
	// is reported here, when the dialog is opened.
	String loadError;
	if (Merge20LearnGetLoadError(loadError))
	{
		SetDlgItemText(IDC_MERGE20_LEARN_LOAD_ERROR,
			strutils::format_string1(_("Could not load the learning file: %1"), loadError).c_str());
		GetDlgItem(IDC_MERGE20_LEARN_LOAD_ERROR)->ShowWindow(SW_SHOW);
	}
	else
	{
		GetDlgItem(IDC_MERGE20_LEARN_LOAD_ERROR)->ShowWindow(SW_HIDE);
	}

	// Sources (HTML `learnSources()`, most judgments first), with the
	// rule / judgment counts each source contributes.
	m_sourcesList.ResetContent();
	m_sourceIds.clear();
	const std::vector<Merge20LearnSourceSummary> summaries = store.GetSources();
	for (const Merge20LearnSourceSummary& summary : summaries)
	{
		m_sourcesList.AddString(
			strutils::format_string3(_("%1 (rules: %2, judgments: %3)"),
				summary.name, strutils::to_str(summary.rules), strutils::to_str(summary.n)).c_str());
		m_sourceIds.push_back(summary.id);
	}
	if (summaries.empty())
		m_sourcesList.AddString(_("No learning data yet.").c_str());

	// Rules (HTML `learnList`): most judgments first, top 50. The
	// dominant strategy and its share come from Merge20LearnDominant,
	// the same function the matcher uses, so the list and the
	// candidate display can never disagree. The share is computed at
	// run time as an integer ratio, rounded like the HTML tool
	// (`Math.round(share * 100)`); no share is stored in the file.
	m_rulesList.ResetContent();
	std::vector<const Merge20LearnRule*> rules;
	rules.reserve(store.Rules().size());
	for (const auto& kv : store.Rules())
		rules.push_back(&kv.second);
	std::sort(rules.begin(), rules.end(),
		[](const Merge20LearnRule* a, const Merge20LearnRule* b) { return a->n > b->n; });
	if (rules.empty())
	{
		m_rulesList.AddString(_("No learning data yet. Judge a diff in the result pane to record it.").c_str());
	}
	else
	{
		size_t shown = 0;
		for (const Merge20LearnRule* pRule : rules)
		{
			if (shown >= kMaxRulesShown)
				break;
			Merge20LearnStrategy dominant = Merge20LearnStrategy::KeepNew;
			int best = 0;
			int total = 0;
			if (!Merge20LearnDominant(*pRule, dominant, best, total))
				continue;
			++shown;
			const bool isExact = pRule->key.rfind(_T("EXACT|"), 0) == 0;
			String keyText = isExact ? pRule->key.substr(6) : pRule->key;
			if (isExact && keyText.length() > kMaxExactKeyChars)
				keyText = keyText.substr(0, kMaxExactKeyChars);
			const int percent = (best * 100 + total / 2) / total;
			const String share = strutils::format_string2(_("%1 times / %2%"),
				strutils::to_str(best), strutils::to_str(percent));
			const String via = Merge20LearnViaDisplayName(isExact ? _T("exact") : _T("feat"));
			const String strategy = Merge20LearnStrategyDisplayName(dominant);
			const String* ruleArgs[] = { &via, &keyText, &strategy, &share };
			m_rulesList.AddString(
				strutils::format_strings(_("[%1] %2 - %3 (%4)"), ruleArgs, 4).c_str());
		}
	}

	OnSelChangeSources();
}

void Merge20LearnDlg::OnToggle()
{
	GetOptionsMgr()->SaveOption(OPT_MERGE20_LEARNING_ENABLED, !Merge20LearnIsEnabled());
	// Switching OFF clears the candidates of all open docs, switching
	// ON recomputes them (L2, design section 3.5).
	Merge20LearnRefreshAllOpenDocs();
	RefreshAll();
}

void Merge20LearnDlg::OnDeleteSource()
{
	const int sel = m_sourcesList.GetCurSel();
	if (sel < 0 || sel >= static_cast<int>(m_sourceIds.size()))
		return;
	const String srcId = m_sourceIds[static_cast<size_t>(sel)];
	const Merge20LearnStore& store = Merge20LearnGetStore();
	String srcName = srcId;
	for (const Merge20LearnSourceSummary& summary : store.GetSources())
	{
		if (summary.id == srcId)
		{
			srcName = summary.name;
			break;
		}
	}
	// HTML `deleteLearnSource`: only this source's contribution is
	// deleted; the other sources keep their judgments.
	if (AfxMessageBox(
			strutils::format_string1(_("Delete the learning data of \"%1\"? (Only this source's contribution is deleted.)"), srcName).c_str(),
			MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;
	Merge20LearnStore& mutableStore = Merge20LearnGetStore();
	mutableStore.RemoveSource(srcId);
	String errStr;
	if (!Merge20LearnSaveStore(errStr))
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
	// The deletion may remove rules other docs matched; recompute.
	Merge20LearnRefreshAllOpenDocs();
	RefreshAll();
}

void Merge20LearnDlg::OnSelChangeSources()
{
	const int sel = m_sourcesList.GetCurSel();
	GetDlgItem(IDC_MERGE20_LEARN_DELETE_SOURCE)->EnableWindow(
		sel >= 0 && sel < static_cast<int>(m_sourceIds.size()) ? TRUE : FALSE);
}
