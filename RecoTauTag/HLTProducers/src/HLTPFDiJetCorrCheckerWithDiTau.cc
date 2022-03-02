/** Description: Check correlation between PFJet pairs and filtered PFTau pairs and store the PFJet pairs.
For (j1, j2, t1, t2) where j1, j2 from the PFJet collection and t1, t2 from the filtered PFTau collection,
the module checks if there is no overlap (within dRmin) between j1, j2, t1, t2, i.e. they are 4 different objects.
In addition, the module imposes the following cuts:
* mjjMin: the min invariant mass cut on (j1, j2)
* extraTauPtCut: the leading tau pt cut on (t1, t2) (under the assumption t1, t2 are products of a subleading pt filter with minN = 2)
The module stores j1, j2 of any (j1, j2, t1, t2) that satisfies the conditions above. */

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"
#include "Math/GenVector/VectorUtil.h"
#include "DataFormats/HLTReco/interface/TriggerTypeDefs.h"
#include "DataFormats/TauReco/interface/PFTau.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "CommonTools/Utils/interface/PtComparator.h"

//
// class definition
//
class HLTPFDiJetCorrCheckerWithDiTau : public edm::stream::EDProducer<> {
private:
  const edm::EDGetTokenT<trigger::TriggerFilterObjectWithRefs> tauSrc_;
  const edm::EDGetTokenT<reco::PFJetCollection> pfJetSrc_;
  const double extraTauPtCut_;
  const double mjjMin_;
  const double matchingR2_;
  // pt comparator
  GreaterByPt<reco::PFJet> pTComparator_;

public:
  explicit HLTPFDiJetCorrCheckerWithDiTau(const edm::ParameterSet&);
  ~HLTPFDiJetCorrCheckerWithDiTau() override;
  void produce(edm::Event&, const edm::EventSetup&) override;
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);
};

//
// class declaration
//
HLTPFDiJetCorrCheckerWithDiTau::HLTPFDiJetCorrCheckerWithDiTau(const edm::ParameterSet& iConfig)
    : tauSrc_(consumes(iConfig.getParameter<edm::InputTag>("TauSrc"))),
      pfJetSrc_(consumes(iConfig.getParameter<edm::InputTag>("PFJetSrc"))),
      extraTauPtCut_(iConfig.getParameter<double>("extraTauPtCut")),
      mjjMin_(iConfig.getParameter<double>("mjjMin")),
      matchingR2_(std::pow(iConfig.getParameter<double>("dRmin"), 2)) {
  produces<reco::PFJetCollection>();
}
HLTPFDiJetCorrCheckerWithDiTau::~HLTPFDiJetCorrCheckerWithDiTau() {}

void HLTPFDiJetCorrCheckerWithDiTau::produce(edm::Event& iEvent, const edm::EventSetup& iES) {
  std::unique_ptr<reco::PFJetCollection> cleanedPFJets(new reco::PFJetCollection);

  edm::Handle<reco::PFJetCollection> pfJets;
  iEvent.getByToken(pfJetSrc_, pfJets);

  trigger::VRpftau taus;
  iEvent.get(tauSrc_).getObjects(trigger::TriggerTau, taus);

  std::set<unsigned int> indices;

  if (pfJets->size() > 1 && taus.size() > 1) {
    for (unsigned int iJet1 = 0; iJet1 < pfJets->size(); iJet1++) {
      for (unsigned int iJet2 = iJet1 + 1; iJet2 < pfJets->size(); iJet2++) {
        bool correctComb = false;
        const reco::PFJet& myPFJet1 = (*pfJets)[iJet1];
        const reco::PFJet& myPFJet2 = (*pfJets)[iJet2];

        if ((myPFJet1.p4() + myPFJet2.p4()).M() < mjjMin_)
          continue;

        for (unsigned int iTau1 = 0; iTau1 < taus.size(); iTau1++) {
          if (reco::deltaR2(taus[iTau1]->p4(), myPFJet1.p4()) < matchingR2_)
            continue;
          if (reco::deltaR2(taus[iTau1]->p4(), myPFJet2.p4()) < matchingR2_)
            continue;

          for (unsigned int iTau2 = iTau1 + 1; iTau2 < taus.size(); iTau2++) {
            if (reco::deltaR2(taus[iTau2]->p4(), myPFJet1.p4()) < matchingR2_)
              continue;
            if (reco::deltaR2(taus[iTau2]->p4(), myPFJet2.p4()) < matchingR2_)
              continue;

            if (taus[iTau1]->pt() < extraTauPtCut_ && taus[iTau2]->pt() < extraTauPtCut_)
              continue;

            correctComb = true;
          }
        }

        if (correctComb) {
          indices.insert(iJet1);
          indices.insert(iJet2);
        }
      }
    }

    for (const auto& i : indices)
      cleanedPFJets->push_back((*pfJets)[i]);
  }
  // sort jets in pt
  std::sort(cleanedPFJets->begin(), cleanedPFJets->end(), pTComparator_);
  iEvent.put(std::move(cleanedPFJets));
}

void HLTPFDiJetCorrCheckerWithDiTau::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("PFJetSrc", edm::InputTag("hltAK4PFJetsCorrected"))->setComment("Input collection of PFJets");
  desc.add<edm::InputTag>("TauSrc", edm::InputTag("hltSinglePFTau20TrackPt1LooseChargedIsolationReg"))
      ->setComment("Input collection of PFTaus that have passed ID and isolation requirements");
  desc.add<double>("extraTauPtCut", 45)->setComment("In case of asymmetric tau pt cuts");
  desc.add<double>("mjjMin", 500)->setComment("VBF dijet mass condition");
  desc.add<double>("dRmin", 0.5)->setComment("Minimum dR between PFJets and filtered PFTaus");
  descriptions.setComment(
      "This module produces a collection of PFJets that are cross-cleaned with respect to PFTaus passing a HLT "
      "filter.");
  descriptions.add("HLTPFDiJetCorrCheckerWithDiTau", desc);
}

//
// module registration
//
#include "FWCore/PluginManager/interface/ModuleDef.h"
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(HLTPFDiJetCorrCheckerWithDiTau);
