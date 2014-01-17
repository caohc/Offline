#include "Mu2eUtilities/inc/SimParticleTimeOffset.hh"

#include "cetlib/exception.h"

#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Principal/Event.h"

#include "MCDataProducts/inc/StepPointMC.hh"


namespace mu2e {

  SimParticleTimeOffset::SimParticleTimeOffset(const fhicl::ParameterSet& pset) {
    // Do not add any offsets by default
    if(!pset.is_empty()) {
      typedef std::vector<std::string> VS;
      VS in(pset.get<VS>("inputs"));
      for(const auto& i : in) {
        inputs_.emplace_back(i);
      }
    }
  }

  void SimParticleTimeOffset::updateMap(const art::Event& evt) {
    offsets_.clear();
    for(const auto& tag: inputs_) {
      auto m = evt.getValidHandle<SimParticleTimeMap>(tag);
      offsets_.emplace_back(*m);
    }
  }

  double SimParticleTimeOffset::totalTimeOffset(art::Ptr<SimParticle> p) const {
    // Navigate to the primary
    while(p->parent()) {
      p = p->parent();
    }

    // Look up primary in all the maps, and add up the offsets
    double dt = 0;
    for(const auto& m : offsets_) {
      const auto it = m.find(p);
      if(it != m.end()) {
        dt += it->second;
      }
      else {
        throw cet::exception("BADINPUTS")
          <<"SimParticleTimeOffset::totalTimeOffset(): the primary "<<p
          <<" is not in an input map\n";
      }
    }
    return dt;
  }

  double SimParticleTimeOffset::totalTimeOffset(const StepPointMC& s) const {
    return totalTimeOffset(s.simParticle());
  }

  double SimParticleTimeOffset::timeWithOffsetsApplied(const StepPointMC& s) const {
    return s.time() + totalTimeOffset(s);
  }

}