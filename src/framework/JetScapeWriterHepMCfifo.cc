#include "JetScapeWriterHepMCfifo.h"
#include "JetScapeLogger.h"
#include "HardProcess.h"
#include "JetScapeSignalManager.h"
#include "GTL/node.h"
#include <GTL/topsort.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

using HepMC3::Units;

using namespace Jetscape;

// namespace Jetscape {

RegisterJetScapeModule<JetScapeWriterHepMCfifo> JetScapeWriterHepMCfifo::reg(
    "CustomWriterJetScapeHepMCfifo");


JetScapeWriterHepMCfifo::~JetScapeWriterHepMCfifo() {
  if (GetActive())
    Close();
}

void JetScapeWriterHepMCfifo::WriteHeaderToFile() {
  // Create event here - not actually writing
  // TODO: GeV seems right, but I don't think we actually measure in mm
  // Should multiply all lengths by 1e-12 probably
  evt = GenEvent(Units::GEV, Units::MM);

  // Expects pb, pythia delivers mb
  auto xsec = make_shared<HepMC3::GenCrossSection>();
  xsec->set_cross_section(GetHeader().GetSigmaGen() * 1e9, 0);
  xsec->set_cross_section(GetHeader().GetSigmaGen() * 1e9,
                          GetHeader().GetSigmaErr() * 1e9);
  evt.set_cross_section(xsec);
  evt.weights().push_back(GetHeader().GetEventWeight());

  auto heavyion = make_shared<HepMC3::GenHeavyIon>();
  // see https://gitlab.cern.ch/hepmc/HepMC3/blob/master/include/HepMC/GenHeavyIon.h
  if (GetHeader().GetNpart() > -1) {
    // Not clear what the difference is...
    heavyion->Ncoll_hard = GetHeader().GetNcoll();
    heavyion->Ncoll = GetHeader().GetNcoll();
  }
  if (GetHeader().GetNcoll() > -1) {
    // Hepmc separates into target and projectile.
    // Set one? Which? Both? half to each? setting projectile for now.
    // setting both might lead to weird problems when they get added up
    heavyion->Npart_proj = GetHeader().GetNpart();
  }
  if (GetHeader().GetTotalEntropy() > -1) {
    // nothing good in the HepMC standard. Something related to mulitplicity would work
  }

  if (GetHeader().GetEventPlaneAngle() > -999) {
    heavyion->event_plane_angle = GetHeader().GetEventPlaneAngle();
  }

  evt.set_heavy_ion(heavyion);

  // also a good moment to initialize the hadron boolean
}

void JetScapeWriterHepMCfifo::WriteEvent() {
  VERBOSE(1) << "Run JetScapeWriterHepMCfifo: Write event # " << GetCurrentEvent();
  
  // Have collected all vertices now.
  // Add all vertices to the event
  for (auto v : vertices){
    evt.add_vertex(v);
  }

  VERBOSE(1) << " found " << vertices.size() << " vertices in the list";

  // If there are no hadrons, promote final partons
  // The graph support of hepmc is a bit rudimentary.
  // easiest is to just check all childless particles
  // Note, one could just check for status==11,
  // but modules are allowed to assign that number to non-final partons
  if ( !hashadrons ) {
    VERBOSE(1) << " found no hadrons, promoting final partons to status 1";
    for ( auto p : evt.particles() ){
      if ( p->children().size() == 0 ){
	if ( p->status() !=11 ){
	  JSWARN << "Found a final parton with status!=11 : status=" << p->status() << ". This should not happen";
	}
	p->set_status(1);
      }
    }
  }
  evt.set_event_number(GetCurrentEvent());
  write_event(evt);
    //   write_event(evt);
  vertices.clear();
  hadronizationvertex = 0;
}

//This function dumps the particles in a specific parton shower to the event
void JetScapeWriterHepMCfifo::Write(weak_ptr<PartonShower> ps) {
  shared_ptr<PartonShower> pShower = ps.lock();
  if (!pShower)
    return;

  // Need topological order, see
  // https://hepmc.web.cern.ch/hepmc/differences.html
  // That means if parton p1 comes into vertex v, and p2 goes out of v,
  // then p1 has to be created (bestowed an id) before p2
  // Take inspiration from hepmc3.0.0/interfaces/pythia8/src/Pythia8ToHepMC3.cc
  // But pythia showers are different from our existing graph structure,
  // So instead try to modify the first attempt to respect top. order
  // and don't create vertices and particles more than once

  // Using GTL's topsort
  // 1. Check that our graph is sane
  if (!pShower->is_acyclic())
    throw std::runtime_error(
        "PROBLEM in JetScapeWriterHepMCfifo: Graph is not acyclic.");

  // 2.
  topsort topsortsearch;
  topsortsearch.scan_whole_graph(true);
  topsortsearch.start_node(); // defaults to first node
  topsortsearch.run(*pShower);
  auto nEnd =
      topsortsearch.top_order_end(); // this is a topsort::topsort_iterator

  // Need to keep track of already created ones
  map<int, GenParticlePtr> CreatedPartons;

  // probably unnecessary, used for consistency checks
  map<int, bool> vused;
  bool foundRoot = false;
  for (auto nIt = topsortsearch.top_order_begin(); nIt != nEnd; ++nIt) {
    // cout << *nIt << "  " << nIt->indeg() << "  " << nIt->outdeg() << endl;

    // Should be the only time we see this node.
    if (vused.find(nIt->id()) != vused.end()){
      throw std::runtime_error( "PROBLEM in JetScapeWriterHepMCfifo: Reusing a vertex.");
    }
    vused[nIt->id()] = true;

    // 0. No incoming edges?
    // ---------------------
    // This is typically a shower initiator.
    // HepMC needs an incoming and outgoing particle for every vertex.
    // That could be a place to attach partons or ions.
    // We could also do both, but as of now, JETSCAPE actually attaches a dummy vertex
    // to the start of the initiator, that can safely go away.
    // Previously, we attached a dummy or clone of the outgoing one here.
    // Instead, we can just skip the vertex. Its outgoing edges will be picked up
    // as incomers in a later vertex.
    // Note that the [0]=>[1] connection in JETSCAPE
    // already uses a dummy node[0], and [1] is at time t=0; removing that seems correct.
    if (nIt->indeg() == 0)    continue;

    // 1. Create a new vertex.
    // --------------------------------------------
    auto v = castVtxToHepMC(pShower->GetVertex(*nIt));

    // 2. Incoming edges
    // -----------------
    //  In the current framework, it should only be one.
    //  So we will catch anything more but provide a mechanism that should work anyway.
    if (nIt->indeg() > 1) {
      JSWARN << "Found more than one mother parton! Should only happen if we "
	"added medium particles. "
	     << "The code should work, but proceed with caution";
    }

    auto inIt = nIt->in_edges_begin();
    auto inEnd = nIt->in_edges_end();
    for (/* nop */; inIt != inEnd; ++inIt) {
      auto phepin = CreatedPartons.find(inIt->id());
      if (phepin != CreatedPartons.end()) {
	// We should already have one!
	v->add_particle_in(phepin->second);
      } else {
	// This indicates we skipped an earlier vertex without incomers.
	// JSWARN << "Incoming particle out of nowhere. This could maybe happen "
	//           "if we pick up medium particles "
	//        << " but is probably a topsort problem. Try using the code "
	//           "after this throw() but be very careful.";
	// throw std::runtime_error("PROBLEM in JetScapeWriterHepMCfifo: Incoming "
	//                          "particle out of nowhere.");
	
	auto in = pShower->GetParton(*inIt);
	auto hepin = castPartonToHepMC(in);
	auto status = std::abs(hepin->status());
	if ( status < 11 || status > 200) {
	  // incoming edge can't be final
	  status = 12;
	}
	hepin->set_status(status);
	CreatedPartons[inIt->id()] = hepin;
	v->add_particle_in(hepin);
	
	if ( nIt->outdeg() == 0 ) {
	  // However, motherless AND childless particles do exist
	  // I.e., a shower initiator that never actually showers
	  // For this, we need an out going clone, much like 3) below
	  auto hepout = castPartonToHepMC(in);
	  // Since the status information is preserved in the incomer, we'll force 11
	  // Note: if we later see in WriteEvent() that there are no hadrons, this will be overwritten to 1
	  hepout->set_status( 11 );
	  // Note that we do not register this particle. Since it's pointing nowhere it can never be reused.
	  v->add_particle_out(hepout);
	} 
      }
    }

    // 3. Outgoing edges?
    // --------------------------------------------
    // 3.1: No. Need to create one.
    // We'll use this opportunity to copy the incomer but give it a final code
    if (nIt->outdeg() == 0) {
      if (nIt->indeg() != 1) {
	// This won't work with multiple incomers (but that's pretty unphysical)
        throw std::runtime_error("PROBLEM in JetScapeWriterHepMCfifo: Need exactly "
                                 "one parent to clone final state partons.");
      }
      auto in = pShower->GetParton(*(nIt->in_edges_begin()));
      auto hepout = castPartonToHepMC(in);
      // an outgoing edge without terminator is "final"
      // Since the status information is preserved in the incomer, we'll force 11
      // Note: if we later see in WriteEvent() that there are no hadrons, this will be overwritten to 1
      hepout->set_status( 11 );
      v->add_particle_out(hepout);
      // Note that we do not register this particle. Since it's pointing nowhere it can never be reused.
    }

    // 3.2: Otherwise use and register the outgoing edge
    if (nIt->outdeg() > 0) {
      auto outIt = nIt->out_edges_begin();
      auto outEnd = nIt->out_edges_end();
      for (/* nop */; outIt != outEnd; ++outIt) {
        if (CreatedPartons.find(outIt->id()) != CreatedPartons.end()) {
          throw std::runtime_error("PROBLEM in JetScapeWriterHepMCfifo: Trying to "
                                   "recreate a preexisting GenParticle.");
        }
        auto out = pShower->GetParton(*outIt);
        auto hepout = castPartonToHepMC(out);
	if ( !hepout->status()) {
	  // incoming and outgoing -> status 12
	  hepout->set_status(12);
	}
	
        CreatedPartons[outIt->id()] = hepout;
        v->add_particle_out(hepout);
      }
    }
    
    vertices.push_back(v);
  }
}

void JetScapeWriterHepMCfifo::Write(weak_ptr<Hadron> h) {
  auto hadron = h.lock();
  if (!hadron)
    return;

  // No clear source for most hadrons
  // Also, a graph with e.g. recombination hadrons would have loops,
  // (though the direction should still make it acyclic?)
  // Not sure how this is supposed to be done in HepMC3
  // Our solution: Attach all hadrons to one dedicated hadronization vertex.
  // Future option: Have separate shower and bulk vertices?

  // Create if it doesn't exist yet
  if (!hadronizationvertex) {
    // dummy position
    HepMC3::FourVector vtxPosition(0, 0, 0, 100); // set it to a late time...
    hadronizationvertex = make_shared<GenVertex>(vtxPosition);

    // dummy mother -- could also maybe use the first/hardest shower initiator
    HepMC3::FourVector pmom(0, 0, 0, 0);
    make_shared<GenParticle>(pmom, 0, 0);
    hadronizationvertex->add_particle_in(make_shared<GenParticle>(pmom, 0, 0));

    vertices.push_back(hadronizationvertex);
    hashadrons=true;
  }

  // now attach
  auto hepmc = castHadronToHepMC(hadron);
  if ( !hepmc->status() ) {
    // unless otherwise specified, all hadrons get status 1
    // TODO: Need to better account for short-lived hadrons
    hepmc->set_status(1);
  }
  hadronizationvertex->add_particle_out(hepmc);
}

void JetScapeWriterHepMCfifo::Init() {
  // if (GetActive()) {
  //   JSINFO << "JetScapeWriterHepMCfifo initialized with output file = "
  //          << GetOutputFileName();
  //   // Create the output file
  //   if(mkfifo(GetOutputFileName().c_str(), 0666) < 0) {
  //     if (errno != EEXIST) {
  //       JSINFO << "Error creating FIFO: " << strerror(errno);
  //       throw std::runtime_error("Error creating FIFO");
  //     }
  //   }
  // }
   if (GetActive()) {
    JSINFO << "JetScape HepMC Writer initialized with output file = "
           << GetOutputFileName();
  }
}

void JetScapeWriterHepMCfifo::Exec() {
  // Nothing to do
}

// }