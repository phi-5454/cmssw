/** \class HLTCaloJetTimingProducer
 *
 *  \brief  This produces timing and associated ecal cell information for calo jets 
 *  \author Matthew Citron
 *
 */

// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "DataFormats/Common/interface/ValueMap.h"

#include "DataFormats/JetReco/interface/CaloJetCollection.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"
#include "DataFormats/Math/interface/deltaR.h"

//
// class declaration
//
class HLTCaloJetTimingProducer : public edm::stream::EDProducer<> {
public:
  explicit HLTCaloJetTimingProducer(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void produce(edm::Event&, const edm::EventSetup&) override;
  void jetTimeFromEcalCells(const reco::CaloJet&,
                            const edm::SortedCollection<EcalRecHit, edm::StrictWeakOrdering<EcalRecHit>>&,
                            float&,
                            float&,
                            uint&);

  // Input collections
  const edm::EDGetTokenT<reco::CaloJetCollection> jetInputToken_;
  const edm::EDGetTokenT<edm::SortedCollection<EcalRecHit, edm::StrictWeakOrdering<EcalRecHit>>> ecalRecHitsEBToken_;
  const edm::EDGetTokenT<edm::SortedCollection<EcalRecHit, edm::StrictWeakOrdering<EcalRecHit>>> ecalRecHitsEEToken_;

  // Include barrel, endcap jets or both
  const bool barrelJets_;
  const bool endcapJets_;
  const double ecalCellEnergyThresh_;
  const double ecalCellTimeThresh_;
  const double ecalCellTimeErrorThresh_;
  const double matchingRadius2_;

  edm::ESHandle<CaloGeometry> _pG;
};

//Constructor
HLTCaloJetTimingProducer::HLTCaloJetTimingProducer(const edm::ParameterSet& iConfig)
    : jetInputToken_{consumes<std::vector<reco::CaloJet>>(iConfig.getParameter<edm::InputTag>("jets"))},
      ecalRecHitsEBToken_{consumes<edm::SortedCollection<EcalRecHit, edm::StrictWeakOrdering<EcalRecHit>>>(
          iConfig.getParameter<edm::InputTag>("ebRecHitsColl"))},
      ecalRecHitsEEToken_{consumes<edm::SortedCollection<EcalRecHit, edm::StrictWeakOrdering<EcalRecHit>>>(
          iConfig.getParameter<edm::InputTag>("eeRecHitsColl"))},
      barrelJets_{iConfig.getParameter<bool>("barrelJets")},
      endcapJets_{iConfig.getParameter<bool>("endcapJets")},
      ecalCellEnergyThresh_{iConfig.getParameter<double>("ecalCellEnergyThresh")},
      ecalCellTimeThresh_{iConfig.getParameter<double>("ecalCellTimeThresh")},
      ecalCellTimeErrorThresh_{iConfig.getParameter<double>("ecalCellTimeErrorThresh")},
      matchingRadius2_{iConfig.getParameter<double>("matchingRadius2")} {
  produces<edm::ValueMap<float>>("");
  produces<edm::ValueMap<unsigned int>>("jetCellsForTiming");
  produces<edm::ValueMap<float>>("jetEcalEtForTiming");
}

//calculateJetTime
void HLTCaloJetTimingProducer::jetTimeFromEcalCells(
    const reco::CaloJet& jet,
    const edm::SortedCollection<EcalRecHit, edm::StrictWeakOrdering<EcalRecHit>>& ecalRecHits,
    float& weightedTimeCell,
    float& totalEmEnergyCell,
    uint& nCells) {
  for (auto const& ecalRH : ecalRecHits) {
    if (ecalRH.checkFlag(EcalRecHit::kSaturated) || ecalRH.checkFlag(EcalRecHit::kLeadingEdgeRecovered) ||
        ecalRH.checkFlag(EcalRecHit::kPoorReco) || ecalRH.checkFlag(EcalRecHit::kWeird) ||
        ecalRH.checkFlag(EcalRecHit::kDiWeird))
      continue;
    if (ecalRH.energy() < ecalCellEnergyThresh_)
      continue;
    if (ecalRH.timeError() <= 0. || ecalRH.timeError() > ecalCellTimeErrorThresh_)
      continue;
    if (fabs(ecalRH.time()) > ecalCellTimeThresh_)
      continue;
    auto const pos = _pG->getPosition(ecalRH.detid());
    if (reco::deltaR2(jet, pos) > matchingRadius2_)
      continue;
    weightedTimeCell += ecalRH.time() * ecalRH.energy() * sin(pos.theta());
    totalEmEnergyCell += ecalRH.energy() * sin(pos.theta());
    nCells++;
  }
  if (totalEmEnergyCell > 0) {
    weightedTimeCell /= totalEmEnergyCell;
  }
}

//Producer
void HLTCaloJetTimingProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  edm::Handle<reco::CaloJetCollection> jets;
  iEvent.getByToken(jetInputToken_, jets);
  auto const& ecalRecHitsEB = iEvent.get(ecalRecHitsEBToken_);
  auto const& ecalRecHitsEE = iEvent.get(ecalRecHitsEEToken_);

  std::vector<float> jetTimings;
  std::vector<unsigned int> jetCellsForTiming;
  std::vector<float> jetEcalEtForTiming;

  jetTimings.reserve(jets->size());
  jetEcalEtForTiming.reserve(jets->size());
  jetCellsForTiming.reserve(jets->size());

  iSetup.get<CaloGeometryRecord>().get(_pG);
  for (auto const& jet : *jets) {
    float weightedTimeCell = 0;
    float totalEmEnergyCell = 0;
    unsigned int nCells = 0;
    if (barrelJets_)
      jetTimeFromEcalCells(jet, ecalRecHitsEB, weightedTimeCell, totalEmEnergyCell, nCells);
    if (endcapJets_) {
      weightedTimeCell *= totalEmEnergyCell;
      jetTimeFromEcalCells(jet, ecalRecHitsEE, weightedTimeCell, totalEmEnergyCell, nCells);
    }

    // If there is at least one ecal cell passing selection, calculate timing
    jetTimings.emplace_back(totalEmEnergyCell > 0 ? weightedTimeCell : -50);
    jetEcalEtForTiming.emplace_back(totalEmEnergyCell);
    jetCellsForTiming.emplace_back(nCells);
  }

  std::unique_ptr<edm::ValueMap<float>> jetTimings_out(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler jetTimings_filler(*jetTimings_out);
  jetTimings_filler.insert(jets, jetTimings.begin(), jetTimings.end());
  jetTimings_filler.fill();
  iEvent.put(std::move(jetTimings_out), "");

  std::unique_ptr<edm::ValueMap<float>> jetEcalEtForTiming_out(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler jetEcalEtForTiming_filler(*jetEcalEtForTiming_out);
  jetEcalEtForTiming_filler.insert(jets, jetEcalEtForTiming.begin(), jetEcalEtForTiming.end());
  jetEcalEtForTiming_filler.fill();
  iEvent.put(std::move(jetEcalEtForTiming_out), "jetEcalEtForTiming");

  std::unique_ptr<edm::ValueMap<unsigned int>> jetCellsForTiming_out(new edm::ValueMap<unsigned int>());
  edm::ValueMap<unsigned int>::Filler jetCellsForTiming_filler(*jetCellsForTiming_out);
  jetCellsForTiming_filler.insert(jets, jetCellsForTiming.begin(), jetCellsForTiming.end());
  jetCellsForTiming_filler.fill();
  iEvent.put(std::move(jetCellsForTiming_out), "jetCellsForTiming");
}

// Fill descriptions
void HLTCaloJetTimingProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("jets", edm::InputTag(""));
  desc.add<bool>("barrelJets", false);
  desc.add<bool>("endcapJets", false);
  desc.add<double>("ecalCellEnergyThresh", 0.5);
  desc.add<double>("ecalCellTimeThresh", 12.5);
  desc.add<double>("ecalCellTimeErrorThresh", 100.);
  desc.add<double>("matchingRadius2", 0.16);
  desc.add<edm::InputTag>("ebRecHitsColl", edm::InputTag("hltEcalRecHit", "EcalRecHitsEB"));
  desc.add<edm::InputTag>("eeRecHitsColl", edm::InputTag("hltEcalRecHit", "EcalRecHitsEE"));
  descriptions.addWithDefaultLabel(desc);
}

// declare this class as a framework plugin
DEFINE_FWK_MODULE(HLTCaloJetTimingProducer);
