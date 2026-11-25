/**
 * @file  Merge20FileStates.cpp
 *
 * @brief Implementation of the Merge20 per-file state sidecar map (S1).
 */
#include "pch.h"
#include "Merge20/Merge20FileStates.h"

const Merge20FileState Merge20FileStates::s_defaultState{};

const Merge20FileState& Merge20FileStates::GetState(const String& relPath) const
{
	auto it = m_states.find(relPath);
	if (it == m_states.end())
		return s_defaultState;
	return it->second;
}

Merge20FileState& Merge20FileStates::GetOrCreateState(const String& relPath)
{
	return m_states[relPath];
}

ReviewState Merge20FileStates::GetReviewState(const String& relPath) const
{
	return GetState(relPath).review;
}

void Merge20FileStates::SetReviewState(const String& relPath, ReviewState state)
{
	GetOrCreateState(relPath).review = state;
}

bool Merge20FileStates::HasState(const String& relPath) const
{
	return m_states.find(relPath) != m_states.end();
}

bool Merge20FileStates::GetIncludeInOutput(const String& relPath) const
{
	return GetState(relPath).includeInOutput;
}

void Merge20FileStates::SetIncludeInOutput(const String& relPath, bool include)
{
	GetOrCreateState(relPath).includeInOutput = include;
}

String Merge20FileStates::GetMemo(const String& relPath) const
{
	return GetState(relPath).memo;
}

void Merge20FileStates::SetMemo(const String& relPath, const String& memo)
{
	GetOrCreateState(relPath).memo = memo;
}
