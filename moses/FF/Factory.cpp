#include "moses/FF/Factory.h"
#include "moses/StaticData.h"

#include "moses/TranslationModel/PhraseDictionaryTreeAdaptor.h"
#include "moses/TranslationModel/RuleTable/PhraseDictionaryOnDisk.h"
#include "moses/TranslationModel/PhraseDictionaryMemory.h"
#include "moses/TranslationModel/CompactPT/PhraseDictionaryCompact.h"
#include "moses/TranslationModel/PhraseDictionaryMultiModel.h"
#include "moses/TranslationModel/PhraseDictionaryMultiModelCounts.h"
#include "moses/TranslationModel/RuleTable/PhraseDictionaryALSuffixArray.h"
#include "moses/TranslationModel/PhraseDictionaryDynSuffixArray.h"

#include "moses/LexicalReordering.h"

#include "moses/FF/BleuScoreFeature.h"
#include "moses/FF/TargetWordInsertionFeature.h"
#include "moses/FF/SourceWordDeletionFeature.h"
#include "moses/FF/GlobalLexicalModel.h"
#include "moses/FF/GlobalLexicalModelUnlimited.h"
#include "moses/FF/UnknownWordPenaltyProducer.h"
#include "moses/FF/WordTranslationFeature.h"
#include "moses/FF/TargetBigramFeature.h"
#include "moses/FF/TargetNgramFeature.h"
#include "moses/FF/PhraseBoundaryFeature.h"
#include "moses/FF/PhrasePairFeature.h"
#include "moses/FF/PhraseLengthFeature.h"
#include "moses/FF/DistortionScoreProducer.h"
#include "moses/FF/WordPenaltyProducer.h"
#include "moses/FF/InputFeature.h"
#include "moses/FF/PhrasePenalty.h"
#include "moses/FF/OSM-Feature/OpSequenceModel.h"

#include "moses/LM/Ken.h"
#ifdef LM_IRST
#include "moses/LM/IRST.h"
#endif

#ifdef LM_SRI
#include "moses/LM/SRI.h"
#endif

#ifdef LM_RAND
#include "moses/LM/Rand.h"
#endif

#ifdef HAVE_SYNLM
#include "moses/SyntacticLanguageModel.h"
#endif

#include "util/exception.hh"

#include <vector>

namespace Moses {

class FeatureFactory {
  public:
    virtual ~FeatureFactory() {}

    virtual void Create(const std::string &line) = 0;

  protected:
    template <class F> static void DefaultSetup(F *feature);

    FeatureFactory() {}
};

template <class F> void FeatureFactory::DefaultSetup(F *feature) {
  StaticData &static_data = StaticData::InstanceNonConst();
  static_data.SetWeights(feature, static_data.GetParameter()->GetWeights(feature->GetScoreProducerDescription()));
}

namespace {

template <class F> class DefaultFeatureFactory : public FeatureFactory {
  public:
    void Create(const std::string &line) {
      DefaultSetup(new F(line));
    }
};

class KenFactory : public FeatureFactory {
  public:
    void Create(const std::string &line) {
      DefaultSetup(ConstructKenLM(line));
    }
};

//WTF(hieu): unknown word should be a normal feature
class UnknownFactory : public FeatureFactory {
  public:
    void Create(const std::string &line) {
      StaticData &static_data = StaticData::InstanceNonConst();
      UnknownWordPenaltyProducer *f = new UnknownWordPenaltyProducer(line);
      std::vector<float> weights = static_data.GetParameter()->GetWeights(f->GetScoreProducerDescription());
      if (weights.empty())
        weights.push_back(1.0f);
      static_data.SetWeights(f, weights);
    }
};

#ifdef LM_RAND
class RandFactory : public FeatureFactory {
  public:
    void Create(const std::string &line) {
      DefaultSetup(NewRandLM());
    }
};
#endif

} // namespace

FeatureRegistry::FeatureRegistry() {
// Feature with same name as class
#define MOSES_FNAME(name) Add(#name, new DefaultFeatureFactory< name >());
// Feature with different name than class.
#define MOSES_FNAME2(name, type) Add(name, new DefaultFeatureFactory< type >());
  MOSES_FNAME(GlobalLexicalModel);
  //MOSES_FNAME(GlobalLexicalModelUnlimited); This was commented out in the original
  MOSES_FNAME(SourceWordDeletionFeature);
  MOSES_FNAME(TargetWordInsertionFeature);
  MOSES_FNAME(PhraseBoundaryFeature);
  MOSES_FNAME(PhraseLengthFeature);
  MOSES_FNAME(WordTranslationFeature);
  MOSES_FNAME(TargetBigramFeature);
  MOSES_FNAME(TargetNgramFeature);
  MOSES_FNAME(PhrasePairFeature);
  MOSES_FNAME(LexicalReordering);
  MOSES_FNAME2("Generation", GenerationDictionary);
  MOSES_FNAME(BleuScoreFeature);
  MOSES_FNAME2("Distortion", DistortionScoreProducer);
  MOSES_FNAME2("WordPenalty", WordPenaltyProducer);
  MOSES_FNAME(InputFeature);
  MOSES_FNAME2("PhraseDictionaryBinary", PhraseDictionaryTreeAdaptor);
  MOSES_FNAME(PhraseDictionaryOnDisk);
  MOSES_FNAME(PhraseDictionaryMemory);
  MOSES_FNAME(PhraseDictionaryCompact);
  MOSES_FNAME(PhraseDictionaryMultiModel);
  MOSES_FNAME(PhraseDictionaryMultiModelCounts);
  MOSES_FNAME(PhraseDictionaryALSuffixArray);
  MOSES_FNAME(PhraseDictionaryDynSuffixArray);
  MOSES_FNAME(OpSequenceModel);
  MOSES_FNAME(PhrasePenalty);
#ifdef HAVE_SYNLM
  MOSES_FNAME(SyntacticLanguageModel);
#endif
#ifdef LM_IRST
  MOSES_FNAME2("IRSTLM", LanguageModelIRST);
#endif
#ifdef LM_SRI
  MOSES_FNAME2("SRILM", LanguageModelSRI);
#endif
#ifdef LM_RAND
  Add("RANDLM", new RandFactory());
#endif
  Add("KENLM", new KenFactory());
  Add("UnknownWordPenalty", new UnknownFactory());
}

FeatureRegistry::~FeatureRegistry() {}

void FeatureRegistry::Add(const std::string &name, FeatureFactory *factory) {
  std::pair<std::string, boost::shared_ptr<FeatureFactory> > to_ins(name, boost::shared_ptr<FeatureFactory>(factory));
  UTIL_THROW_IF(!registry_.insert(to_ins).second, util::Exception, "Duplicate feature name " << name);
}

namespace {
class UnknownFeatureException : public util::Exception {};
}

void FeatureRegistry::Construct(const std::string &name, const std::string &line) {
  Map::iterator i = registry_.find(name);
  UTIL_THROW_IF(i == registry_.end(), UnknownFeatureException, "Feature name " << name << " is not registered.");
  i->second->Create(line);
}

} // namespace Moses