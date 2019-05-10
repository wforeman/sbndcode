////////////////////////////////////////////////////////////////////////
// Class:       CRTFullRecoAna
// Module Type: analyzer
// File:        CRTFullRecoAna_module.cc
//
// Analysis module for evaluating CRT reconstruction on through going
// muons.
//
// Tom Brooks (tbrooks@fnal.gov)
////////////////////////////////////////////////////////////////////////

// sbndcode includes
#include "sbndcode/RecoUtils/RecoUtils.h"
#include "sbndcode/CRT/CRTProducts/CRTHit.hh"
#include "sbndcode/CRT/CRTProducts/CRTTrack.hh"
#include "sbndcode/CRT/CRTUtils/CRTTruthMatchUtils.h"
#include "sbndcode/Geometry/GeometryWrappers/CRTGeoAlg.h"
#include "sbndcode/Geometry/GeometryWrappers/TPCGeoAlg.h"
#include "sbndcode/CRT/CRTUtils/CRTT0MatchAlg.h"
#include "sbndcode/CRT/CRTUtils/CRTTrackMatchAlg.h"

// LArSoft includes
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/Track.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "larcore/Geometry/Geometry.h"
#include "larcorealg/Geometry/GeometryCore.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"

// Framework includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "canvas/Utilities/Exception.h"
#include "larsim/MCCheater/BackTrackerService.h"
#include "larsim/MCCheater/ParticleInventoryService.h"

// Utility libraries
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/types/Table.h"
#include "fhiclcpp/types/Atom.h"

// ROOT includes. Note: To look up the properties of the ROOT classes,
// use the ROOT web site; e.g.,
// <https://root.cern.ch/doc/master/annotated.html>
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "TVector3.h"
#include "TF1.h"
#include "TString.h"

// C++ includes
#include <map>
#include <vector>
#include <string>

namespace sbnd {

  struct CRTTruthMatch{
    int partID;
    double trueT0;
    std::map<std::string, geo::Point_t> trueCrosses;
    std::map<std::string, bool> validCrosses;
    std::map<std::string, std::vector<crt::CRTHit>> crtHits;
    std::vector<crt::CRTTrack> crtTracks;
    bool hasTpcTrack;
    std::vector<double> hitT0s;
    std::vector<double> trackT0s;
  };

  class CRTFullRecoAna : public art::EDAnalyzer {
  public:

    // Describes configuration parameters of the module
    struct Config {
      using Name = fhicl::Name;
      using Comment = fhicl::Comment;
 
      // One Atom for each parameter
      fhicl::Atom<art::InputTag> SimModuleLabel {
        Name("SimModuleLabel"),
        Comment("tag of detector simulation data product")
      };

      fhicl::Atom<art::InputTag> CRTSimLabel {
        Name("CRTSimLabel"),
        Comment("tag of CRT simulation data product")
      };

      fhicl::Atom<art::InputTag> CRTHitLabel {
        Name("CRTHitLabel"),
        Comment("tag of CRT simulation data product")
      };

      fhicl::Atom<art::InputTag> CRTTrackLabel {
        Name("CRTTrackLabel"),
        Comment("tag of CRT simulation data product")
      };

      fhicl::Atom<art::InputTag> TPCTrackLabel {
        Name("TPCTrackLabel"),
        Comment("tag of TPC track data product")
      };

      fhicl::Atom<bool> Verbose {
        Name("Verbose"),
        Comment("Print information about what's going on")
      };

      fhicl::Table<CRTT0MatchAlg::Config> CRTT0Alg {
        Name("CRTT0Alg"),
      };

      fhicl::Table<CRTTrackMatchAlg::Config> CRTTrackAlg {
        Name("CRTTrackAlg"),
      };

    }; // Config
 
    using Parameters = art::EDAnalyzer::Table<Config>;
 
    // Constructor: configures module
    explicit CRTFullRecoAna(Parameters const& config);
 
    // Called once, at start of the job
    virtual void beginJob() override;
 
    // Called once per event
    virtual void analyze(const art::Event& event) override;

    // Called once, at end of the job
    virtual void endJob() override;

  private:

    // fcl file parameters
    art::InputTag fSimModuleLabel;      ///< name of detsim producer
    art::InputTag fCRTSimLabel;         ///< name of CRT producer
    art::InputTag fCRTHitLabel;         ///< name of CRT producer
    art::InputTag fCRTTrackLabel;       ///< name of CRT producer
    art::InputTag fTPCTrackLabel;       ///< name of CRT producer
    bool          fVerbose;             ///< print information about what's going on
    
    // Histograms
    TH1D* hCRTHitTimes;
    std::map<std::string, TH3D*> hTaggerXYZResolution;
    std::map<std::string, TH1D*> hHitDCA;
    // Number of tracks associated to numu muons
    TH1D* hNuMuCRTTrackMom;

    std::vector<std::string> stage{"CRTHit", "CRTTrack", "HitT0", "TrackT0"};
    std::vector<std::string> category{"PossMatch", "ShouldMatch", "CRTVol", "TPCVol", "CRTCross", "TPCCross"};
    std::vector<std::string> level{"Total", "Matched"};
    TH1D* hEffMom[4][6][2];
    TH1D* hEffTheta[4][6][2];
    TH1D* hEffPhi[4][6][2];

    std::vector<std::string> trackType{"Complete", "Incomplete", "Both"};
    TH1D* hTrackThetaDiff[3];
    TH1D* hTrackPhiDiff[3];

    std::vector<std::string> t0Type{"HitT0", "TrackT0"};
    std::vector<std::string> purity{"Matched", "Correct"};
    TH1D* hPurityMom[2][2];
    TH1D* hPurityTheta[2][2];
    TH1D* hPurityPhi[2][2];

    // Other variables shared between different methods.
    detinfo::DetectorClocks const* fDetectorClocks;
    detinfo::DetectorProperties const* fDetectorProperties;
    TPCGeoAlg fTpcGeo;
    CRTGeoAlg fCrtGeo;
    CRTT0MatchAlg crtT0Alg;
    CRTTrackMatchAlg crtTrackAlg;

  }; // class CRTFullRecoAna

  // Constructor
  CRTFullRecoAna::CRTFullRecoAna(Parameters const& config)
    : EDAnalyzer(config)
    , fSimModuleLabel       (config().SimModuleLabel())
    , fCRTSimLabel          (config().CRTSimLabel())
    , fCRTHitLabel          (config().CRTHitLabel())
    , fCRTTrackLabel        (config().CRTTrackLabel())
    , fTPCTrackLabel        (config().TPCTrackLabel())
    , fVerbose              (config().Verbose())
    , crtT0Alg              (config().CRTT0Alg())
    , crtTrackAlg           (config().CRTTrackAlg())
  {
    // Get a pointer to the fGeometryServiceetry service provider
    fDetectorClocks = lar::providerFrom<detinfo::DetectorClocksService>();
    fDetectorProperties = lar::providerFrom<detinfo::DetectorPropertiesService>();
  }

  void CRTFullRecoAna::beginJob()
  {
    // Access tfileservice to handle creating and writing histograms
    art::ServiceHandle<art::TFileService> tfs;

    hCRTHitTimes = tfs->make<TH1D>("CRTHitTimes","",100, -5000, 5000);

    hNuMuCRTTrackMom = tfs->make<TH1D>("NuMuCRTTrackMom","",20, 0, 10);

    for(size_t i = 0; i < fCrtGeo.NumTaggers(); i++){
      std::string taggerName = fCrtGeo.GetTagger(i).name;
      TString histName = Form("%s_resolution", taggerName.c_str());
      hTaggerXYZResolution[taggerName] = tfs->make<TH3D>(histName, "", 50, -25, 25, 50, -25, 25, 50, -25, 25);
      TString dcaName = Form("%s_dca", taggerName.c_str());
      hHitDCA[taggerName] = tfs->make<TH1D>(dcaName, "", 50, 0, 100);
    }

    for(size_t si = 0; si < stage.size(); si++){
      for(size_t ci = 0; ci < category.size(); ci++){
        for(size_t li = 0; li < level.size(); li++){
          TString momName = Form("MuMom%s_%s_%s", stage[si].c_str(), category[ci].c_str(), level[li].c_str());
          hEffMom[si][ci][li] = tfs->make<TH1D>(momName, "", 20, 0, 10);
          TString thetaName = Form("MuTheta%s_%s_%s", stage[si].c_str(), category[ci].c_str(), level[li].c_str());
          hEffTheta[si][ci][li] = tfs->make<TH1D>(thetaName, "", 20, 0, 3.2);
          TString phiName = Form("MuPhi%s_%s_%s", stage[si].c_str(), category[ci].c_str(), level[li].c_str());
          hEffPhi[si][ci][li] = tfs->make<TH1D>(phiName, "", 20, -3.2, 3.2);
        }
      }
    }

    for(size_t ti = 0; ti < trackType.size(); ti++){
      TString thetaName = Form("TrackThetaDiff%s", trackType[ti].c_str());
      hTrackThetaDiff[ti] = tfs->make<TH1D>(thetaName, "", 50, -0.2, 0.2);
      TString phiName = Form("TrackPhiDiff%s", trackType[ti].c_str());
      hTrackPhiDiff[ti] = tfs->make<TH1D>(phiName, "", 50, -0.2, 0.2);
    }

    for(size_t ti = 0; ti < t0Type.size(); ti++){
      for(size_t pi = 0; pi < purity.size(); pi++){
        TString momName = Form("PurityMom%s_%s", t0Type[ti].c_str(), purity[pi].c_str());
        hPurityMom[ti][pi] = tfs->make<TH1D>(momName, "", 20, 0, 10);
        TString thetaName = Form("PurityTheta%s_%s", t0Type[ti].c_str(), purity[pi].c_str());
        hPurityTheta[ti][pi] = tfs->make<TH1D>(thetaName, "", 20, 0, 3.2);
        TString phiName = Form("PurityPhi%s_%s", t0Type[ti].c_str(), purity[pi].c_str());
        hPurityPhi[ti][pi] = tfs->make<TH1D>(phiName, "", 20, -3.2, 3.2);
      }
    }

    // Initial output
    std::cout<<"----------------- Full CRT Reconstruction Analysis Module -------------------"<<std::endl;

  }// CRTFullRecoAna::beginJob()

  void CRTFullRecoAna::analyze(const art::Event& event)
  {

    // Fetch basic event info
    if(fVerbose){
      std::cout<<"============================================"<<std::endl
               <<"Run = "<<event.run()<<", SubRun = "<<event.subRun()<<", Event = "<<event.id().event()<<std::endl
               <<"============================================"<<std::endl;
    }


    //----------------------------------------------------------------------------------------------------------
    //                                          GETTING PRODUCTS
    //----------------------------------------------------------------------------------------------------------

    // Get g4 particles
    art::ServiceHandle<cheat::ParticleInventoryService> pi_serv;
    auto particleHandle = event.getValidHandle<std::vector<simb::MCParticle>>(fSimModuleLabel);

    // Get CRT hits from the event
    art::Handle< std::vector<crt::CRTHit>> crtHitHandle;
    std::vector<art::Ptr<crt::CRTHit> > crtHitList;
    if (event.getByLabel(fCRTHitLabel, crtHitHandle))
      art::fill_ptr_vector(crtHitList, crtHitHandle);

    // Get CRT tracks from the event
    art::Handle< std::vector<crt::CRTTrack>> crtTrackHandle;
    std::vector<art::Ptr<crt::CRTTrack> > crtTrackList;
    if (event.getByLabel(fCRTTrackLabel, crtTrackHandle))
      art::fill_ptr_vector(crtTrackList, crtTrackHandle);

    // Get reconstructed tracks from the event
    auto tpcTrackHandle = event.getValidHandle<std::vector<recob::Track>>(fTPCTrackLabel);
    art::FindManyP<recob::Hit> findManyHits(tpcTrackHandle, event, fTPCTrackLabel);

    //----------------------------------------------------------------------------------------------------------
    //                                          TRUTH MATCHING
    //----------------------------------------------------------------------------------------------------------
     
    double readoutWindowMuS  = fDetectorClocks->TPCTick2Time((double)fDetectorProperties->ReadOutWindowSize()); // [us]
    double driftTimeMuS = fTpcGeo.MaxX()/fDetectorProperties->DriftVelocity(); // [us]

    std::map<int, simb::MCParticle> particles;
    std::vector<int> lepParticleIds;
    std::map<int, CRTTruthMatch> truthMatching;
    // Loop over the true particles
    for (auto const& particle: (*particleHandle)){
      
      CRTTruthMatch truthMatch;
      // Make map with ID
      int partID = particle.TrackId();
      particles[partID] = particle;
      int pdg = particle.PdgCode();
      
      art::Ptr<simb::MCTruth> truth = pi_serv->TrackIdToMCTruth_P(partID);
      simb::MCNeutrino mcNu = truth->GetNeutrino();
      if(truth->Origin() == simb::kBeamNeutrino){
        geo::Point_t vtx;
        vtx.SetX(mcNu.Nu().Vx()); vtx.SetY(mcNu.Nu().Vy()); vtx.SetZ(mcNu.Nu().Vz());

        if(fTpcGeo.InFiducial(vtx, 0, 0) && std::abs(pdg) == 13 && particle.Mother() == 0){
          lepParticleIds.push_back(partID);
        }
      }
      
      // Only consider muons
      if(std::abs(pdg) != 13) continue;

      double time = particle.T() * 1e-3;
      if(time < -driftTimeMuS || time > readoutWindowMuS) continue;

      truthMatch.partID = partID;
      truthMatch.trueT0 = time;
      truthMatch.hasTpcTrack = false;

      // Loop over the taggers
      for(size_t i = 0; i < fCrtGeo.NumTaggers(); i++){
        std::string taggerName = fCrtGeo.GetTagger(i).name;
        // Find the intersections between the true particle and each tagger (if exists)
        geo::Point_t trueCP = fCrtGeo.TaggerCrossingPoint(taggerName, particle);
        // Make map for each tagger with true ID as key and value an XYZ position
        if(trueCP.X() != -99999){ 
          truthMatch.trueCrosses[taggerName] = trueCP;
        }
        bool validCP = fCrtGeo.ValidCrossingPoint(taggerName, particle);
        truthMatch.validCrosses[taggerName] = validCP;
      } 
      
      truthMatching[partID] = truthMatch;

    }

    //--------------------------------------------- CRT HIT MATCHING -------------------------------------------

    // Loop over CRT hits
    int hit_i = 0;
    std::vector<crt::CRTHit> crtHits;
    for (auto const& crtHit: (*crtHitHandle)){
      crtHits.push_back(crtHit);
      // Get tagger of CRT hit
      std::string taggerName = crtHit.tagger;

      hCRTHitTimes->Fill(crtHit.ts1_ns*1e-3);

      // Get associated true particle
      int partID = CRTTruthMatchUtils::TrueIdFromTotalEnergy(crtHitHandle, event, fCRTHitLabel, fCRTSimLabel, hit_i);
      if(truthMatching.find(partID) == truthMatching.end()){hit_i++; continue;}

      truthMatching[partID].crtHits[taggerName].push_back(crtHit);
      hit_i ++;
    }

    //------------------------------------------- CRT TRACK MATCHING -------------------------------------------

    // Loop over CRT tracks
    int track_i = 0;
    std::vector<crt::CRTTrack> crtTracks;
    for (auto const& crtTrack : (*crtTrackHandle)){
      crtTracks.push_back(crtTrack);

      // Get associated true particle
      int partID = CRTTruthMatchUtils::TrueIdFromTotalEnergy(crtTrackHandle, event, fCRTTrackLabel, fCRTHitLabel, fCRTSimLabel, track_i);
      if(truthMatching.find(partID) == truthMatching.end()){track_i++; continue;}

      truthMatching[partID].crtTracks.push_back(crtTrack);
      track_i++;

      if(std::find(lepParticleIds.begin(), lepParticleIds.end(), partID) != lepParticleIds.end()){
        hNuMuCRTTrackMom->Fill(particles[partID].P());
      }
    }

    //------------------------------------------- CRT T0 MATCHING ----------------------------------------------

    // Loop over reconstructed tracks
    for (auto const& tpcTrack : (*tpcTrackHandle)){
      // Get the associated true particle
      std::vector<art::Ptr<recob::Hit>> hits = findManyHits.at(tpcTrack.ID());
      int partID = RecoUtils::TrueParticleIDFromTotalRecoHits(hits, false);
      if(truthMatching.find(partID) == truthMatching.end()) continue;
      truthMatching[partID].hasTpcTrack = true;

      // Calculate t0 from CRT Hit matching
      int tpc = fTpcGeo.DetectedInTPC(hits);
      double hitT0 = crtT0Alg.T0FromCRTHits(tpcTrack, crtHits, tpc);
      if(hitT0 != -99999) truthMatching[partID].hitT0s.push_back(hitT0);

      // Calculate t0 from CRT Track matching
      double trackT0 = crtTrackAlg.T0FromCRTTracks(tpcTrack, crtTracks, tpc);
      if(trackT0 != -99999) truthMatching[partID].trackT0s.push_back(trackT0);

      std::pair<crt::CRTHit, double> closest = crtT0Alg.ClosestCRTHit(tpcTrack, crtHits, tpc);
      if(closest.second != -99999) hHitDCA[closest.first.tagger]->Fill(closest.second);
    }

    //----------------------------------------------------------------------------------------------------------
    //                                        PERFORMANCE METRICS
    //----------------------------------------------------------------------------------------------------------

    // Loop over all the true particles in the event inside the reco window
    for(auto const& truthMatch : truthMatching){
      CRTTruthMatch match = truthMatch.second;
      int partID = match.partID;

      double momentum = particles[partID].P();
      TVector3 start(particles[partID].Vx(), particles[partID].Vy(), particles[partID].Vz());
      TVector3 end(particles[partID].EndX(), particles[partID].EndY(), particles[partID].EndZ());
      double theta = (end-start).Theta();
      double phi = (end-start).Phi();

      //--------------------------------- CRT HIT RECONSTRUCTION PERFORMANCE -------------------------------------
      // Loop over the true crossing points
      for(auto const& trueCross : match.trueCrosses){
        // Find the closest matched reconstructed hit on that tagger
        std::string taggerName = trueCross.first;
        geo::Point_t trueCP = trueCross.second;

        double minDist = 99999;
        crt::CRTHit closestHit;
        // Loop over the associated CRTHits and find the closest
        if(match.crtHits.find(taggerName) == match.crtHits.end()) continue;
        for(auto const& crtHit : match.crtHits[taggerName]){
          double dist = pow(crtHit.x_pos-trueCP.X(), 2) + pow(crtHit.y_pos-trueCP.Y(), 2) + pow(crtHit.z_pos-trueCP.Z(), 2);
          if(dist < minDist){
            closestHit = crtHit;
            minDist = dist;
          }
        }
        // RESOLUTIONS
        // Fill the histogram of resolutions for that tagger
        if(minDist != 99999){
          hTaggerXYZResolution[taggerName]->Fill(closestHit.x_pos-trueCP.X(), closestHit.y_pos-trueCP.Y(), closestHit.z_pos-trueCP.Z());
        }
      }
      
      //------------------------------- CRT TRACK RECONSTRUCTION PERFORMANCE -------------------------------------
     
      // RESOLUTIONS
      double minThetaDiff = 99999;
      double minPhiDiff = 99999;
      bool complete = true;
      // Get true theta and phi in CRT volume
      simb::MCParticle particle = particles[match.partID];
      TVector3 trueStart(particle.Vx(), particle.Vy(), particle.Vz());
      TVector3 trueEnd(particle.EndX(), particle.EndY(), particle.EndZ());
      if(trueEnd.Y() > trueStart.Y()) std::swap(trueStart, trueEnd);
      double trueTheta = (trueEnd - trueStart).Theta();
      double truePhi = (trueEnd - trueStart).Phi();
      // Loop over the associated CRT tracks and find the one with the closest angle
      for(auto const& crtTrack : match.crtTracks){
        // Take largest y point as the start
        TVector3 recoStart(crtTrack.x1_pos, crtTrack.y1_pos, crtTrack.z1_pos);
        TVector3 recoEnd(crtTrack.x2_pos, crtTrack.y2_pos, crtTrack.z2_pos);
        if(recoEnd.Y() > recoStart.Y()) std::swap(recoStart, recoEnd);
        double recoTheta = (recoEnd - recoStart).Theta();
        double recoPhi = (recoEnd - recoStart).Phi();
        double thetaDiff = TMath::ATan2(TMath::Sin(recoTheta-trueTheta), TMath::Cos(recoTheta-trueTheta));
        double phiDiff = TMath::ATan2(TMath::Sin(recoPhi-truePhi), TMath::Cos(recoPhi-truePhi));
        if(thetaDiff < minThetaDiff){
          minThetaDiff = thetaDiff;
          minPhiDiff = phiDiff;
          complete = crtTrack.complete;
        }
      }
      // Fill theta, phi difference plots for complete/incomplete tracks (maybe consider entry/exit points)
      if(minThetaDiff != 99999){
        if(complete){
          hTrackThetaDiff[0]->Fill(minThetaDiff);
          hTrackPhiDiff[0]->Fill(minPhiDiff);
        }
        else{
          hTrackThetaDiff[1]->Fill(minThetaDiff);
          hTrackPhiDiff[1]->Fill(minPhiDiff);
        }
        hTrackThetaDiff[2]->Fill(minThetaDiff);
        hTrackPhiDiff[2]->Fill(minPhiDiff);
      }
      
      //--------------------------------------- CRT HIT T0 PERFORMANCE -------------------------------------------

      // PURITY
      // Loop over the associated T0s and find the one from the longest reco track
      // If it is within a certain time then fill plots with angles, momentum, reco length
      double bestHitT0Diff = 99999;
      for(auto const& hitT0 : match.hitT0s){
        double hitT0Diff = std::abs(match.trueT0 - hitT0);
        if(hitT0Diff < bestHitT0Diff) bestHitT0Diff = hitT0Diff;
      }
      if(bestHitT0Diff != 99999){
        hPurityMom[0][0]->Fill(momentum);
        hPurityTheta[0][0]->Fill(theta);
        hPurityPhi[0][0]->Fill(phi);
      }
      if(bestHitT0Diff < 2){
        hPurityMom[0][1]->Fill(momentum);
        hPurityTheta[0][1]->Fill(theta);
        hPurityPhi[0][1]->Fill(phi);
      }
     
      //------------------------------------- CRT TRACK T0 PERFORMANCE -------------------------------------------
      // PURITY
      // Loop over the associated T0s and find the one from the longest reco track
      // If it is within a certain time then fill plots with angles, momentum, reco length
      double bestTrackT0Diff = 99999;
      for(auto const& trackT0 : match.trackT0s){
        double trackT0Diff = std::abs(match.trueT0 - trackT0);
        if(trackT0Diff < bestTrackT0Diff) bestTrackT0Diff = trackT0Diff;
      }
      if(bestTrackT0Diff != 99999){
        hPurityMom[1][0]->Fill(momentum);
        hPurityTheta[1][0]->Fill(theta);
        hPurityPhi[1][0]->Fill(phi);
      }
      if(bestTrackT0Diff < 2){
        hPurityMom[1][1]->Fill(momentum);
        hPurityTheta[1][1]->Fill(theta);
        hPurityPhi[1][1]->Fill(phi);
      }

      // --------------------------------- EFFICIENCIES FOR EVERYTHING -------------------------------------------
      // If there are any matched T0s then fill plots with angles, momentum, reco length for different categories
      bool matchesCRTHit = (match.crtHits.size() > 0);
      bool matchesCRTTrack = (match.crtTracks.size() > 0);
      bool matchesHitT0 = (match.hitT0s.size() > 0);
      bool matchesTrackT0 = (match.trackT0s.size() > 0);
      std::vector<bool> matches{matchesCRTHit, matchesCRTTrack, matchesHitT0, matchesTrackT0};

      std::vector<bool> crtHitCategories;
      std::vector<bool> crtTrackCategories;
      std::vector<bool> hitT0Categories;
      std::vector<bool> trackT0Categories;

      // - Possible match (hits & hitT0: cross >= 1 tagger, tracks & trackT0: cross >= 2 taggers)
      crtHitCategories.push_back((match.trueCrosses.size() > 0));
      crtTrackCategories.push_back((match.trueCrosses.size() > 1));
      hitT0Categories.push_back((match.trueCrosses.size() > 0) && match.hasTpcTrack);
      trackT0Categories.push_back((match.trueCrosses.size() > 1) && match.hasTpcTrack);

      // - Should match: (hits: cross 2 perp strips in tagger, tracks: cross 2 perp strips in 2 taggers, 
      //                  hitT0: hit and tpc track associated, trackT0: crt and tpc track associated)
      int nValid = 0;
      for(auto const& valid : match.validCrosses){
        if(valid.second) nValid++;
      }
      crtHitCategories.push_back(nValid > 0);
      crtTrackCategories.push_back(nValid > 1);
      hitT0Categories.push_back((match.crtHits.size() > 0 && match.hasTpcTrack));
      trackT0Categories.push_back((match.crtTracks.size() > 0 && match.hasTpcTrack));

      // - Enters CRT volume
      bool entersCRT = fCrtGeo.EntersVolume(particles[partID]);
      crtHitCategories.push_back(entersCRT);
      crtTrackCategories.push_back(entersCRT);
      hitT0Categories.push_back((entersCRT && match.hasTpcTrack));
      trackT0Categories.push_back((entersCRT && match.hasTpcTrack));

      // - Enters TPC volume
      bool entersTPC = fTpcGeo.EntersVolume(particles[partID]);
      crtHitCategories.push_back(entersTPC);
      crtTrackCategories.push_back(entersTPC);
      hitT0Categories.push_back((entersTPC && match.hasTpcTrack));
      trackT0Categories.push_back((entersTPC && match.hasTpcTrack));

      // - Crosses CRT volume
      bool crossesCRT = fCrtGeo.CrossesVolume(particles[partID]);
      crtHitCategories.push_back(crossesCRT);
      crtTrackCategories.push_back(crossesCRT);
      hitT0Categories.push_back((crossesCRT && match.hasTpcTrack));
      trackT0Categories.push_back((crossesCRT && match.hasTpcTrack));

      // - Crosses TPC volume
      bool crossesTPC = fTpcGeo.CrossesVolume(particles[partID]);
      crtHitCategories.push_back(crossesTPC);
      crtTrackCategories.push_back(crossesTPC);
      hitT0Categories.push_back((crossesTPC && match.hasTpcTrack));
      trackT0Categories.push_back((crossesTPC && match.hasTpcTrack));

      std::vector<std::vector<bool>> matchCategories{crtHitCategories, crtTrackCategories, hitT0Categories, trackT0Categories};

      for(size_t si = 0; si < stage.size(); si++){
        for(size_t ci = 0; ci < category.size(); ci++){
          if(!matchCategories[si][ci]) continue;
          hEffMom[si][ci][0]->Fill(momentum);
          hEffTheta[si][ci][0]->Fill(theta);
          hEffPhi[si][ci][0]->Fill(phi);

          if(!matches[si]) continue;
          hEffMom[si][ci][1]->Fill(momentum);
          hEffTheta[si][ci][1]->Fill(theta);
          hEffPhi[si][ci][1]->Fill(phi);
        }
      }
    }

  } // CRTFullRecoAna::analyze()

  void CRTFullRecoAna::endJob(){

  } // CRTFullRecoAna::endJob()
  
  
  DEFINE_ART_MODULE(CRTFullRecoAna)
} // namespace sbnd


