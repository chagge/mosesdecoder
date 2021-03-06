//
// REFERENCE
// ---------
// When using this feature, please cite: 
// 
// Matthias Huck, Joern Wuebker, Felix Rietig, and Hermann Ney.
// A Phrase Orientation Model for Hierarchical Machine Translation. 
// In ACL 2013 Eighth Workshop on Statistical Machine Translation (WMT 2013), pages 452-463, Sofia, Bulgaria, August 2013. 
//

#pragma once

#include <bitset>
#include <string>
#include <vector>
#include "StatefulFeatureFunction.h"
#include "FFState.h"
#include "moses/Factor.h"
#include "phrase-extract/extract-ghkm/PhraseOrientation.h"


namespace Moses
{

class PhraseOrientationFeatureState : public FFState
{
public:

  friend class PhraseOrientationFeature;

  PhraseOrientationFeatureState() 
    : m_leftBoundaryNonTerminalL2RScores(3,0)
    , m_rightBoundaryNonTerminalR2LScores(3,0)
    , m_leftBoundaryNonTerminalL2RPossibleFutureOrientations(0x7)
    , m_rightBoundaryNonTerminalR2LPossibleFutureOrientations(0x7)
    , m_leftBoundaryRecursionGuard(false)
    , m_rightBoundaryRecursionGuard(false)
    , m_leftBoundaryIsSet(false)
    , m_rightBoundaryIsSet(false)
  {}

  void SetLeftBoundaryL2R(const std::vector<float> &scores,
                          size_t heuristicScoreIndex,
                          std::bitset<3> &possibleFutureOrientations,
                          size_t nonTerminalIndex)
  {
    for (size_t i=0; i<3; ++i) 
    {
      m_leftBoundaryNonTerminalL2RScores[i] = scores[i];
      m_leftBoundaryNonTerminalL2RPossibleFutureOrientations[i] = possibleFutureOrientations[i];
    }
    m_leftBoundaryNonTerminalL2RHeuristicScoreIndex = heuristicScoreIndex;
    m_leftBoundaryNonTerminalIndex = nonTerminalIndex;
    m_leftBoundaryIsSet = true;
  }

  void SetRightBoundaryR2L(const std::vector<float> &scores,
                           size_t heuristicScoreIndex,
                           std::bitset<3> &possibleFutureOrientations,
                           size_t nonTerminalIndex)
  {
    for (size_t i=0; i<3; ++i) 
    {
      m_rightBoundaryNonTerminalR2LScores[i] = scores[i];
      m_rightBoundaryNonTerminalR2LPossibleFutureOrientations[i] = possibleFutureOrientations[i];
    }
    m_rightBoundaryNonTerminalR2LHeuristicScoreIndex = heuristicScoreIndex;
    m_rightBoundaryNonTerminalIndex = nonTerminalIndex;
    m_rightBoundaryIsSet = true;
  }


  float GetLeftBoundaryL2RScoreMono() const 
  {
    return m_leftBoundaryNonTerminalL2RScores[0];
  }

  float GetLeftBoundaryL2RScoreSwap() const 
  {
    return m_leftBoundaryNonTerminalL2RScores[1];
  }

  float GetLeftBoundaryL2RScoreDiscontinuous() const 
  {
    return m_leftBoundaryNonTerminalL2RScores[2];
  }


  float GetRightBoundaryR2LScoreMono() const 
  {
    return m_rightBoundaryNonTerminalR2LScores[0];
  }

  float GetRightBoundaryR2LScoreSwap() const 
  {
    return m_rightBoundaryNonTerminalR2LScores[1];
  }

  float GetRightBoundaryR2LScoreDiscontinuous() const 
  {
    return m_rightBoundaryNonTerminalR2LScores[2];
  }


  int Compare(const FFState& other) const 
  {
    return 0; 
  };

private:

  template<std::size_t N> bool Smaller(const std::bitset<N>& x, const std::bitset<N>& y) const
  {
    for (size_t i=0; i<N; ++i) 
    {
      if (x[i] ^ y[i]) 
        return y[i];
    }
    return false;
  }

  std::vector<float> m_leftBoundaryNonTerminalL2RScores;
  std::vector<float> m_rightBoundaryNonTerminalR2LScores;

  size_t m_leftBoundaryNonTerminalL2RHeuristicScoreIndex;
  size_t m_rightBoundaryNonTerminalR2LHeuristicScoreIndex;

  std::bitset<3> m_leftBoundaryNonTerminalL2RPossibleFutureOrientations;
  std::bitset<3> m_rightBoundaryNonTerminalR2LPossibleFutureOrientations;

  size_t m_leftBoundaryNonTerminalIndex;
  size_t m_rightBoundaryNonTerminalIndex;
  bool m_leftBoundaryRecursionGuard;
  bool m_rightBoundaryRecursionGuard;
  bool m_leftBoundaryIsSet;
  bool m_rightBoundaryIsSet;
};



class PhraseOrientationFeature : public StatefulFeatureFunction
{
public:

  PhraseOrientationFeature(const std::string &line);

  ~PhraseOrientationFeature() {
  }

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }

  virtual const FFState* EmptyHypothesisState(const InputType &input) const {
    return new PhraseOrientationFeatureState();
  }

  void SetParameter(const std::string& key, const std::string& value);

  void EvaluateInIsolation(const Phrase &source
                , const TargetPhrase &targetPhrase
                , ScoreComponentCollection &scoreBreakdown
                , ScoreComponentCollection &estimatedFutureScore) const
  {
    targetPhrase.SetRuleSource(source);
  };

  void EvaluateWithSourceContext(const InputType &input
                , const InputPath &inputPath
                , const TargetPhrase &targetPhrase
                , const StackVec *stackVec
                , ScoreComponentCollection &scoreBreakdown
                , ScoreComponentCollection *estimatedFutureScore = NULL) const
  {};

  FFState* EvaluateWhenApplied(
    const Hypothesis& cur_hypo,
    const FFState* prev_state,
    ScoreComponentCollection* accumulator) const 
  {
    return new PhraseOrientationFeatureState();
  };

  FFState* EvaluateWhenApplied(
    const ChartHypothesis& cur_hypo,
    int featureID, // used to index the state in the previous hypotheses
    ScoreComponentCollection* accumulator) const;

protected:

  void LeftBoundaryL2RScoreRecursive(int featureID,
                                     const ChartHypothesis *hypo, 
                                     const PhraseOrientationFeatureState *state, 
                                     const std::bitset<3> orientation,
                                     std::vector<float>& newScores) const;

  void RightBoundaryR2LScoreRecursive(int featureID,
                                      const ChartHypothesis *hypo, 
                                      const PhraseOrientationFeatureState *state, 
                                      const std::bitset<3> orientation, 
                                      std::vector<float>& newScores) const;

  std::string m_glueTargetLHSStr;
  Word m_glueTargetLHS;
  size_t m_offsetR2LScores;

};


}

