//
// A module to create CRV photons arriving at the SiPMs (using StepPointMCs)
//
//
// Original Author: Ralf Ehrlich

#include "Offline/CRVConditions/inc/CRVDigitizationPeriod.hh"
#include "Offline/CRVConditions/inc/CRVPhotonYield.hh"
#include "Offline/CRVResponse/inc/MakeCrvPhotons.hh"
#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/DataProducts/inc/CRSScintillatorBarIndex.hh"
#include "Offline/DataProducts/inc/CRVId.hh"

#include "Offline/GlobalConstantsService/inc/GlobalConstantsHandle.hh"
#include "Offline/GlobalConstantsService/inc/ParticleDataList.hh"
#include "Offline/GlobalConstantsService/inc/PhysicsParams.hh"
#include "Offline/ConfigTools/inc/ConfigFileLookupPolicy.hh"
#include "Offline/GeometryService/inc/DetectorSystem.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/GeometryService/inc/GeometryService.hh"
#include "Offline/MCDataProducts/inc/CrvStep.hh"
#include "Offline/MCDataProducts/inc/CrvPhotons.hh"
#include "Offline/MCDataProducts/inc/ProtonBunchTimeMC.hh"
#include "Offline/SeedService/inc/SeedService.hh"

#include "Offline/ProditionsService/inc/ProditionsHandle.hh"
#include "Offline/DAQConditions/inc/EventTiming.hh"
#include "Offline/DataProducts/inc/EventWindowMarker.hh"

#include "canvas/Persistency/Common/Ptr.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Core/EDAnalyzer.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Sequence.h"
#include "CLHEP/Units/GlobalSystemOfUnits.h"
#include "CLHEP/Random/Randomize.h"

#include <string>
#include <filesystem>
#include <set>
#include <boost/functional/hash.hpp>

#include <TDirectory.h>
#include <TFile.h>
#include <TTree.h>
#include <TMath.h>


namespace mu2e
{
  class CrvPhotonGenerator : public art::EDProducer
  {

    public:
    using Name=fhicl::Name;
    using Comment=fhicl::Comment;
    struct Config
    {
      fhicl::Atom<int> debug{ Name("debugLevel"),Comment("Debug Level"), 0};
      fhicl::Sequence<std::string> moduleLabels{ Name("crvStepModuleLabels"), Comment("CrvStepModule labels")};
      fhicl::Sequence<std::string> processNames{ Name("crvStepProcessNames"), Comment("process names of CrvSteps")};
      fhicl::Sequence<std::string> CRVSectors{ Name("CRVSectors"), Comment("Crv sectors")};
      fhicl::Sequence<int> reflectors{ Name("reflectors"), Comment("location of reflectors at Crv sectors")};
      fhicl::Sequence<std::string> lookupTableFileNames{ Name("lookupTableFileNames"), Comment("lookup tables for Crv sectors")};
      fhicl::Sequence<double> scintillationYields{ Name("scintillationYields"), Comment("scintillation yields at Crv sectors")};
      fhicl::Atom<double> photonYieldScaleFactor{ Name("photonYieldScaleFactor"), Comment("global scale factor for the photon yield")};
      fhicl::Atom<double> photonYieldVariationScale{ Name("photonYieldVariationScale"),Comment("scale factor of the photon yield variation")};
      fhicl::Atom<double> photonYieldVariationCutoffLow{ Name("photonYieldVariationCutoffLow"),Comment("lower cutoff at photon yield variation")};
      fhicl::Atom<double> photonYieldVariationCutoffHigh{ Name("photonYieldVariationCutoffHigh"),Comment("upper cutoff at photon yield variation")};
      fhicl::Atom<double> digitizationStart{ Name("digitizationStart"), Comment("start of digitization after DAQ event window start")};
      fhicl::Atom<double> digitizationEnd{ Name("digitizationEnd"), Comment("end of digitization after DAQ event window start")};
      fhicl::Atom<double> digitizationStartMargin{ Name("digitizationStartMargin"),
                               Comment("time window before digitization starts to account for photon travel time and electronics response.")};
      fhicl::Atom<int> numberSamplesNZS{Name("numberSamplesNZS")}; //134
      fhicl::Atom<bool> simulateNZS{Name("simulateNZS")}; //false
      fhicl::Atom<art::InputTag> eventWindowMarkerTag{ Name("eventWindowMarkerTag"), Comment("EventWindowMarker producer"),"EWMProducer" };
      fhicl::Atom<art::InputTag> protonBunchTimeMCTag{ Name("protonBunchTimeMCTag"), Comment("ProtonBunchTimeMC producer"),"EWMProducer" };
    };
    using Parameters = art::EDProducer::Table<Config>;
    explicit CrvPhotonGenerator(const Parameters& conf);
    void produce(art::Event& e);
    void beginRun(art::Run& r);

    private:
    int _debug;
    std::vector<std::string> _moduleLabels;
    std::vector<std::string> _processNames;
    std::vector<std::unique_ptr<art::Selector> > _selectors;

    ConfigFileLookupPolicy                                     _resolveFullPath;
    std::vector<std::string>                                   _CRVSectors;
    std::vector<int>                                           _reflectors;
    std::vector<std::string>                                   _lookupTableFileNames;
    std::vector<double>                                        _scintillationYields;
    std::vector<boost::shared_ptr<mu2eCrv::MakeCrvPhotons> >   _makeCrvPhotons;

    double                                       _photonYieldScaleFactor;
    mu2e::ProditionsHandle<mu2e::CRVPhotonYield> _photonYieldVariationVector;
    double                                       _photonYieldVariationScale;
    double                                       _photonYieldVariationCutoffLow;
    double                                       _photonYieldVariationCutoffHigh;

    //On-spill
    //-Event length: 1695ns (microbunch period)
    //-Digitization window
    //---first DAQ clock tick after POT occurs between 0ns and 25ns after protons.
    //   25ns jitter is due to the microbunch period not being an integer multiple of the DAQ clock period.
    //---event window starts 200ns after these first DAQ clock ticks (--> 200ns...225ns after POT).
    //   (this delay is the same the tracker is using)
    //---due to the above variations, the event window length varies between 1675ns and 1700ns.
    //---digitization start: 200ns after event window start (--> 400ns...425ns after POT)
    //---digitization end: 1500ns after event window start (--> 1700ns...1725ns after POT),
    //   which is 1300ns after digitization start
    //-CrvSteps
    //---to speed up the simulation for ZS data:
    //------CrvSteps before 100ns before the digitization window are removed.
    //      (--> 100ns after event window start, 300ns...325ns after POT)
    //      to account for photon travel time and electronics response time.
    //------CrvSteps after the digitization window are removed.
    //------only for the removal decision: apply a time wrapping (modulus microbunch period of 1695ns)
    //      at the event window starts, to avoid breaking up hits,
    //      because these CrvSteps are far away from the digitization window.
    //      this time wrapping will not be applied to the photon generation,
    //      where the un-wrapped time will be used.
    //-CrvPhotons
    //---apply a time wrapping (modulus microbunch period of 1695ns) at the event window starts,
    //   to avoid breaking up pulses, because these photons are far away from the digitization window
    //
    //Off-spill
    //-Event length: 100000ns
    //-Digitization window
    //---full event length
    //-CrvSteps
    //---record all CrvSteps within event window
    //-CrvPhotons
    //---no time wrapping

    double      _digitizationStart; //200ns after event window start (400ns...425ns after POT)
    double      _digitizationEnd;   //1500ns after event window start (1700ns...1725ns after POT)
    double      _digitizationStartMargin;  //100ns (used to account for photon travel time and electronics response time)
    int         _numberSamplesNZS;  //134
    bool        _simulateNZS;       //false
    art::InputTag _eventWindowMarkerTag;
    art::InputTag _protonBunchTimeMCTag;
    double      _microBunchPeriod;

    CLHEP::HepRandomEngine& _engine;
    CLHEP::RandFlat       _randFlat;
    CLHEP::RandGaussQ     _randGaussQ;
    CLHEP::RandPoissonQ   _randPoissonQ;
  };

  CrvPhotonGenerator::CrvPhotonGenerator(const Parameters& conf) :
    art::EDProducer{conf},
    _debug(conf().debug()),
    _moduleLabels(conf().moduleLabels()),
    _processNames(conf().processNames()),
    _CRVSectors(conf().CRVSectors()),
    _reflectors(conf().reflectors()),
    _lookupTableFileNames(conf().lookupTableFileNames()),
    _scintillationYields(conf().scintillationYields()),
    _photonYieldScaleFactor(conf().photonYieldScaleFactor()),
    _photonYieldVariationScale(conf().photonYieldVariationScale()),
    _photonYieldVariationCutoffLow(conf().photonYieldVariationCutoffLow()),
    _photonYieldVariationCutoffHigh(conf().photonYieldVariationCutoffHigh()),
    _digitizationStart(conf().digitizationStart()),
    _digitizationEnd(conf().digitizationEnd()),
    _digitizationStartMargin(conf().digitizationStartMargin()),
    _numberSamplesNZS(conf().numberSamplesNZS()),
    _simulateNZS(conf().simulateNZS()),
    _eventWindowMarkerTag(conf().eventWindowMarkerTag()),
    _protonBunchTimeMCTag(conf().protonBunchTimeMCTag()),
    _engine{createEngine(art::ServiceHandle<SeedService>()->getSeed())},
    _randFlat(_engine),
    _randGaussQ(_engine),
    _randPoissonQ(_engine)
  {
    //if(_moduleLabels.size()==0) throw std::logic_error("ERROR: a list of crvSteps module labels needs to be provided");
    if(_moduleLabels.size()!=_processNames.size()) throw std::logic_error("ERROR: mismatch between specified selectors (crvStepModuleLabels/crvStepProcessNames)");
    for(size_t i=0; i<_moduleLabels.size(); ++i)
    {
      if(_moduleLabels[i]!="*")
        _selectors.push_back(std::unique_ptr<art::Selector>(new art::Selector(art::ProductInstanceNameSelector("") &&
                                                            art::ModuleLabelSelector(_moduleLabels[i]) &&
                                                            art::ProcessNameSelector(_processNames[i]))));
      else
        _selectors.push_back(std::unique_ptr<art::Selector>(new art::Selector(art::ProductInstanceNameSelector("") &&
                                                            art::ProcessNameSelector(_processNames[i]))));
    }

    if(_lookupTableFileNames.size()!=_CRVSectors.size()) throw std::logic_error("ERROR: mismatch between specified CRV sector names and lookup table list");
    if(_reflectors.size()!=_CRVSectors.size()) throw std::logic_error("ERROR: mismatch between specified CRV sector names and reflector list");
    if(_scintillationYields.size()!=_CRVSectors.size()) throw std::logic_error("ERROR: mismatch between specified CRV sector names and scintillation yield list");

    std::set<std::string> filedirs;
    for(size_t i=0; i<_lookupTableFileNames.size(); ++i)
    {
      bool tableLoaded=false;
      for(size_t j=0; j<i; ++j)
      {
        if(_lookupTableFileNames[i]==_lookupTableFileNames[j])
        {
           tableLoaded=true;
           _makeCrvPhotons.emplace_back(_makeCrvPhotons[j]);
           if(_debug>0) std::cout<<"CRV sector "<<i<<" ("<<_CRVSectors[i]<<") uses "<<_makeCrvPhotons.back()->GetFileName()<<" with scintillation yield of "<<_scintillationYields[i]<<" photons/MeV"<<std::endl;
           break;
        }
      }
      if(tableLoaded) continue;

      _makeCrvPhotons.emplace_back(boost::shared_ptr<mu2eCrv::MakeCrvPhotons>(new mu2eCrv::MakeCrvPhotons(_randFlat, _randGaussQ, _randPoissonQ)));
      boost::shared_ptr<mu2eCrv::MakeCrvPhotons> &photonMaker=_makeCrvPhotons.back();
      std::string filespec = _resolveFullPath(_lookupTableFileNames[i]);
      filedirs.insert( std::filesystem::path(filespec).parent_path() );
      photonMaker->LoadLookupTable(filespec,_debug);
      photonMaker->SetScintillationYield(_scintillationYields[i]);
      if(_debug>0) std::cout<<"CRV sector "<<i<<" ("<<_CRVSectors[i]<<") uses "<<_makeCrvPhotons.back()->GetFileName()<<" with scintillation yield of "<<_scintillationYields[i]<<" photons/MeV"<<std::endl;
    }

    std::cout << "CRV light files:";
    for(auto const& dir : filedirs ){
      std::cout << " " << dir;
    }
    std::cout << std::endl;
    size_t hash = 0;
    for(auto const& mcp: _makeCrvPhotons) {
      boost::hash_combine<std::string>(hash,mcp->GetFileName());
    }
    std::cout << "CRV light sectors: " << _makeCrvPhotons.size() << " hash:" << hash <<std::endl;

    produces<CrvPhotonsCollection>();
  }

  void CrvPhotonGenerator::beginRun(art::Run& rr)
  {
    GeomHandle<CosmicRayShield> CRS;
    std::vector<CRSScintillatorShield> const &shields = CRS->getCRSScintillatorShields();
    if(shields.size()!=_CRVSectors.size()) throw std::logic_error("ERROR: mismatch between the geometry and the specified lookup table CRVSectors");

    for(size_t i=0; i<shields.size(); ++i)
    {
      if(shields[i].getCRSScintillatorBarDetail().getMaterialName()!="G4_POLYSTYRENE")
        throw std::logic_error("ERROR: scintillator material is not the expected G4_POLYSTYRENE which is used in the look-up tables");
      if(shields[i].getName().substr(4)!=_CRVSectors[i]) throw std::logic_error("ERROR: mismatch between the geometry and the specified lookup table CRVSectors");
                            //substr(4) removes the "CRV_" part of the sector name
    }

    _microBunchPeriod = GlobalConstantsHandle<PhysicsParams>()->getNominalDRPeriod();
  }

  void CrvPhotonGenerator::produce(art::Event& event)
  {
    std::unique_ptr<CrvPhotonsCollection> crvPhotonsCollection(new CrvPhotonsCollection);

    std::map<std::pair<mu2e::CRSScintillatorBarIndex,int>,std::vector<CrvPhotons::SinglePhoton> > photonMap;

    auto const& photonYieldVariationVector = _photonYieldVariationVector.get(event.id());

    GeomHandle<CosmicRayShield> CRS;
    GlobalConstantsHandle<ParticleDataList> particleDataList;

    art::Handle<EventWindowMarker> eventWindowMarker;
    event.getByLabel(_eventWindowMarkerTag,eventWindowMarker);
    EventWindowMarker::SpillType spillType = eventWindowMarker->spillType();
    double eventWindowLength = eventWindowMarker->eventLength(); //onspill: 1675ns/1700ns, offspill: 100000ns

    //offspill
    double eventWindowStart=0;
    double startTime=0;
    double endTime=eventWindowLength;

    //onspill
    if(spillType==EventWindowMarker::SpillType::onspill)
    {
      art::Handle<ProtonBunchTimeMC> protonBunchTimeMC;
      event.getByLabel(_protonBunchTimeMCTag, protonBunchTimeMC);
      eventWindowStart = -protonBunchTimeMC->pbtime_; //200ns...225ns

      startTime=eventWindowStart+_digitizationStart-_digitizationStartMargin; //300ns...325ns
      endTime=eventWindowStart+_digitizationEnd; //1700ns...1725ns

      if(_simulateNZS)
      {
        startTime=eventWindowStart-_digitizationStartMargin; //100ns...125ns
        endTime=eventWindowStart+_numberSamplesNZS*CRVDigitizationPeriod; //1875ns...1900s
      }
    }

    for(size_t j=0; j<_selectors.size(); ++j)
    {
      std::vector<art::Handle<CrvStepCollection> > CrvStepsVector = event.getMany<CrvStepCollection>(*(_selectors.at(j)));
      for(size_t i=0; i<CrvStepsVector.size(); ++i)
      {
        const art::Handle<CrvStepCollection> &CrvSteps = CrvStepsVector[i];
        for(size_t istep=0; istep<CrvSteps->size(); ++istep)
        {
          CrvStep const& step(CrvSteps->at(istep));

          double t1 = step.startTime();
          double t2 = step.endTime();
          if(isnan(t1) || isnan(t2)) continue;  //This situation was observed once. Not sure how it happened.

          if(!_simulateNZS)  //no steps will be removed for NZS
          {
            //time wrap around eventWindowStart which is far away from any steps that need to be used
            double t1Tmp = fmod(t1-eventWindowStart,_microBunchPeriod)+eventWindowStart;
            double t2Tmp = fmod(t2-eventWindowStart,_microBunchPeriod)+eventWindowStart;
            //fmod of a negative number returns a negative number
            if(t1Tmp<eventWindowStart) t1Tmp+=_microBunchPeriod;
            if(t2Tmp<eventWindowStart) t2Tmp+=_microBunchPeriod;
            //remove steps that are not needed to speed up the simulation
            if(t1Tmp<startTime && t2Tmp<startTime) continue;
            if(t1Tmp>endTime && t2Tmp>endTime) continue;
            if(t1Tmp>endTime && t2Tmp<startTime) continue;  //if the step's t1Tmp/t2Tmp times are just before/after the eventWindowStart and get time-wrapped in a weird way
          }

          CLHEP::Hep3Vector pos1 = step.startPosition();  //TODO: Need to convert everything into XYZVec, so that const references can be used
          CLHEP::Hep3Vector pos2 = step.endPosition();

          int PDGcode = step.simParticle()->pdgId();
          auto const& particle = particleDataList->particle(PDGcode);
          double mass = particle.mass();  //MeV/c^2
          double charge = particle.charge(); //in units of elementary charges

          double energy1   = sqrt(step.startMom().mag2() + mass*mass); //MeV
          double energy2   = sqrt(step.endMom()*step.endMom() + mass*mass);
          double avgEnergy = 0.5*(energy1+energy2);
          double avgGamma  = avgEnergy/mass;
          double avgBeta   = sqrt(1.0-1.0/(avgGamma*avgGamma));

          const CRSScintillatorBar &CRSbar = CRS->getBar(step.barIndex());
          const CLHEP::Hep3Vector &pos1Local = CRSbar.toLocal(pos1);
          const CLHEP::Hep3Vector &pos2Local = CRSbar.toLocal(pos2);

          const CRSScintillatorBarId &barId = CRSbar.id();
          int CRVSectorNumber=barId.getShieldNumber();

          boost::shared_ptr<mu2eCrv::MakeCrvPhotons> &photonMaker=_makeCrvPhotons.at(CRVSectorNumber);

          //get the channel-specific deviation of the light yield (total from scintillation and Cerenkov) from the nominal value,
          //e.g. due to scintillator variations or SiPM misalignments
          for(size_t SiPM=0; SiPM<CRVId::nChanPerBar; ++SiPM)
          {
            size_t channel = step.barIndex().asUint()*CRVId::nChanPerBar + SiPM;
            float photonYieldDeviation = photonYieldVariationVector.photonYieldDeviation(channel);
            photonYieldDeviation *= _photonYieldVariationScale;  //scale factor for the variation
            if(photonYieldDeviation<_photonYieldVariationCutoffLow) photonYieldDeviation=_photonYieldVariationCutoffLow;
            if(photonYieldDeviation>_photonYieldVariationCutoffHigh) photonYieldDeviation=_photonYieldVariationCutoffHigh;
            photonYieldDeviation = (photonYieldDeviation+1.0)*_photonYieldScaleFactor;  //global photon yield scale factor for e.g. aging
            photonMaker->SetPhotonYieldDeviation(photonYieldDeviation,SiPM);
          }

          photonMaker->MakePhotons(pos1Local, pos2Local, t1, t2,
                                        avgBeta, charge,
                                        step.visibleEDep(),
                                        step.pathLength(),
                                        _reflectors[CRVSectorNumber]);

          art::Ptr<CrvStep> crvStepPtr(CrvSteps,istep);
          for(size_t SiPM=0; SiPM<CRVId::nChanPerBar; ++SiPM)
          {
            std::pair<CRSScintillatorBarIndex,int> barIndexSiPMNumber(step.barIndex(),SiPM);
            const std::vector<double> &times=photonMaker->GetArrivalTimes(SiPM);
            if(times.empty()) continue;
            std::vector<CrvPhotons::SinglePhoton> &photons = photonMap[barIndexSiPMNumber];
            for(size_t itime=0; itime<times.size(); ++itime)
            {
              double timeTmp=times[itime];
              if(spillType==EventWindowMarker::SpillType::onspill)
              {
                //time wrap around eventWindowStart which is far away from the (ZS) digi window to avoid breaking (ZS) pulses apart
                timeTmp = fmod(timeTmp-eventWindowStart,_microBunchPeriod)+eventWindowStart;
                //fmod of a negative number returns a negative number
                if(timeTmp<eventWindowStart) timeTmp+=_microBunchPeriod;
              }
              photons.emplace_back(timeTmp,crvStepPtr);
            }
          }

        } //loop over StepPointMCs in the StepPointMC collection
      } //loop over all StepPointMC collections
    } //loop over all module labels / process names from the fcl file

    for(auto p=photonMap.begin(); p!=photonMap.end(); ++p)
    {
      crvPhotonsCollection->emplace_back(p->first.first,p->first.second,p->second);
    }

    event.put(std::move(crvPhotonsCollection));
  }

}

using mu2e::CrvPhotonGenerator;
DEFINE_ART_MODULE(CrvPhotonGenerator)
