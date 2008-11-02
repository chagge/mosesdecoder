// $Id: HypothesisStackNormal.cpp 1511 2007-11-12 20:21:44Z hieuhoang1972 $

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#include <algorithm>
#include <set>
#include <queue>
#include "HypothesisStackNormal.h"
#include "TypeDef.h"
#include "Util.h"
#include "StaticData.h"

using namespace std;

namespace Moses
{
HypothesisStackNormal::HypothesisStackNormal()
{
	m_nBestIsEnabled = StaticData::Instance().IsNBestEnabled();
	m_bestScore = -std::numeric_limits<float>::infinity();
	m_worstScore = -std::numeric_limits<float>::infinity();
}

/** remove all hypotheses from the collection */
void HypothesisStackNormal::RemoveAll()
{
	while (m_hypos.begin() != m_hypos.end())
	{
		Remove(m_hypos.begin());
	}
}

pair<HypothesisStackNormal::iterator, bool> HypothesisStackNormal::Add(Hypothesis *hypo)
{
	std::pair<iterator, bool> ret = m_hypos.insert(hypo);
	if (ret.second) 
	{ // equiv hypo doesn't exists
		VERBOSE(3,"added hyp to stack");
	
		// Update best score, if this hypothesis is new best
		if (hypo->GetTotalScore() > m_bestScore)
		{
			VERBOSE(3,", best on stack");
			m_bestScore = hypo->GetTotalScore();
			// this may also affect the worst score
	        if ( m_bestScore + m_beamWidth > m_worstScore )
	          m_worstScore = m_bestScore + m_beamWidth;
					}
	
	    // Prune only if stack is twice as big as needed (lazy pruning)
		VERBOSE(3,", now size " << m_hypos.size());
		if (m_hypos.size() > 2*m_maxHypoStackSize-1)
		{
			PruneToSize(m_maxHypoStackSize);
		}
		else {
		  VERBOSE(3,std::endl);
		}
	}	
	
	return ret;
}

bool HypothesisStackNormal::AddPrune(Hypothesis *hypo)
{ 
	if (hypo->GetTotalScore() < m_worstScore)
	{ // really bad score. don't bother adding hypo into collection
	  StaticData::Instance().GetSentenceStats().AddDiscarded();
	  VERBOSE(3,"discarded, too bad for stack" << std::endl);
		FREEHYPO(hypo);		
		return false;
	}

	// over threshold, try to add to collection
	std::pair<iterator, bool> addRet = Add(hypo); 
	if (addRet.second)
	{ // nothing found. add to collection
		return true;
  }

	// equiv hypo exists, recombine with other hypo
	iterator &iterExisting = addRet.first;
	Hypothesis *hypoExisting = *iterExisting;
	assert(iterExisting != m_hypos.end());
	 
	 
	const WordsRange currSourceWordsRange  = hypo->GetCurrSourceWordsRange();
	const WordsRange existSourceWordsRange = hypoExisting->GetCurrSourceWordsRange();
	
	/**
	 * 	if the two hypotheses differ in range and one of them follows a gap, do not recombine! May be needed for maxent scoring
	 */
	
	if( StaticData::Instance().UseMaxentReordering()){
		if( existSourceWordsRange.GetStartPos() != 0 && !(hypoExisting->GetWordsBitmap().GetValue( existSourceWordsRange.GetStartPos()-1)) ){	
			if(currSourceWordsRange.GetNumWordsCovered() != existSourceWordsRange.GetNumWordsCovered()){
				IFVERBOSE(2){
					std::cerr << "Words ranges are different and existing hypo is following a gap --> do not recombine hypotheses..\n";
				}
				return true;
			}
		} 
		else if( currSourceWordsRange.GetStartPos() != 0 && !(hypo->GetWordsBitmap().GetValue( currSourceWordsRange.GetStartPos()-1)) ){
			if(currSourceWordsRange.GetNumWordsCovered() != existSourceWordsRange.GetNumWordsCovered()){
				IFVERBOSE(2){
					std::cerr << "Words ranges are different and new hypo is following a gap --> do not recombine hypotheses..\n";
				}
				return true;
			}
		}
	}

	StaticData::Instance().GetSentenceStats().AddRecombination(*hypo, **iterExisting);
	
	// found existing hypo with same target ending.
	// keep the best 1
	if (hypo->GetTotalScore() > hypoExisting->GetTotalScore())
	{ // incoming hypo is better than the one we have
		VERBOSE(3,"better than matching hyp " << hypoExisting->GetId() << ", recombining, ");
		if (m_nBestIsEnabled) {
			hypo->AddArc(hypoExisting);
			Detach(iterExisting);
		} else {
			Remove(iterExisting);
		}

		bool added = Add(hypo).second;		
		if (!added)
		{
			iterExisting = m_hypos.find(hypo);
			TRACE_ERR("Offending hypo = " << **iterExisting << endl);
			abort();
		}
		return false;
	}
	else
	{ // already storing the best hypo. discard current hypo 
	  VERBOSE(3,"worse than matching hyp " << hypoExisting->GetId() << ", recombining" << std::endl)
		if (m_nBestIsEnabled) {
			hypoExisting->AddArc(hypo);
		} else {
			FREEHYPO(hypo);				
		}
		return false;
	}
}

void HypothesisStackNormal::PruneToSize(size_t newSize)
{
	if (m_hypos.size() > newSize) // ok, if not over the limit
	{
		priority_queue<float> bestScores;
		
		// push all scores to a heap
		// (but never push scores below m_bestScore+m_beamWidth)
		iterator iter = m_hypos.begin();
		float score = 0;
		while (iter != m_hypos.end())
		{
			Hypothesis *hypo = *iter;
			score = hypo->GetTotalScore();
			if (score > m_bestScore+m_beamWidth) 
			{
				bestScores.push(score);
			}
			++iter;
    }
		
		// pop the top newSize scores (and ignore them, these are the scores of hyps that will remain)
		//  ensure to never pop beyond heap size
		size_t minNewSizeHeapSize = newSize > bestScores.size() ? bestScores.size() : newSize;
		for (size_t i = 1 ; i < minNewSizeHeapSize ; i++)
			bestScores.pop();
				
		// and remember the threshold
		float scoreThreshold = bestScores.top();
		
		// delete all hypos under score threshold
		iter = m_hypos.begin();
		while (iter != m_hypos.end())
		{
			Hypothesis *hypo = *iter;
			float score = hypo->GetTotalScore();
			if (score < scoreThreshold)
			{
				iterator iterRemove = iter++;
				Remove(iterRemove);
				StaticData::Instance().GetSentenceStats().AddPruning();
			}
			else
			{
				++iter;
			}
		}
		VERBOSE(3,", pruned to size " << size() << endl);
		
		IFVERBOSE(3) 
		{
			TRACE_ERR("stack now contains: ");
			for(iter = m_hypos.begin(); iter != m_hypos.end(); iter++) 
			{
				Hypothesis *hypo = *iter;
				TRACE_ERR( hypo->GetId() << " (" << hypo->GetTotalScore() << ") ");
			}
			TRACE_ERR( endl);
		}

		// set the worstScore, so that newly generated hypotheses will not be added if worse than the worst in the stack
		m_worstScore = scoreThreshold;
	}
}

const Hypothesis *HypothesisStackNormal::GetBestHypothesis() const
{
	if (!m_hypos.empty())
	{
		const_iterator iter = m_hypos.begin();
		Hypothesis *bestHypo = *iter;
		while (++iter != m_hypos.end())
		{
			Hypothesis *hypo = *iter;
			if (hypo->GetTotalScore() > bestHypo->GetTotalScore())
				bestHypo = hypo;
		}
		return bestHypo;
	}
	return NULL;
}

vector<const Hypothesis*> HypothesisStackNormal::GetSortedList() const
{
	vector<const Hypothesis*> ret; ret.reserve(m_hypos.size());
	std::copy(m_hypos.begin(), m_hypos.end(), std::inserter(ret, ret.end()));
	sort(ret.begin(), ret.end(), CompareHypothesisTotalScore());

	return ret;
}


void HypothesisStackNormal::CleanupArcList()
{
	// only necessary if n-best calculations are enabled
	if (!m_nBestIsEnabled) return;

	iterator iter;
	for (iter = m_hypos.begin() ; iter != m_hypos.end() ; ++iter)
	{
		Hypothesis *mainHypo = *iter;
		mainHypo->CleanupArcList();
	}
}

TO_STRING_BODY(HypothesisStackNormal);


// friend
std::ostream& operator<<(std::ostream& out, const HypothesisStackNormal& hypoColl)
{
	HypothesisStackNormal::const_iterator iter;
	
	for (iter = hypoColl.begin() ; iter != hypoColl.end() ; ++iter)
	{
		const Hypothesis &hypo = **iter;
		out << hypo << endl;
		
	}
	return out;
}


}

