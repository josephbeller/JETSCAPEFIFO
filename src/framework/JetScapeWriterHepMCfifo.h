#ifndef JETSCAPEWRITERHEPMCFIFO_H
#define JETSCAPEWRITERHEPMCFIFO_H

#include <fstream>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

#include "JetScapeWriter.h"
#include "PartonShower.h"

#include "HepMC3/GenEvent.h"
#include "HepMC3/ReaderAscii.h"
#include "HepMC3/WriterAscii.h"
#include "HepMC3/Print.h"


// using namespace HepMC;
using HepMC3::GenEvent;
using HepMC3::GenVertex;
using HepMC3::GenParticle;
using HepMC3::GenVertexPtr;
using HepMC3::GenParticlePtr;

// using namespace Jetscape;
using namespace std;



namespace Jetscape {


class JetScapeWriterHepMCfifo : public JetScapeWriter, public HepMC3::WriterAscii {

public:
  JetScapeWriterHepMCfifo() : HepMC3::WriterAscii("") { SetId("HepMCfifio writer"); };
  JetScapeWriterHepMCfifo(string m_file_name_out)
     : JetScapeWriter(m_file_name_out), HepMC3::WriterAscii(m_file_name_out) {
    SetId("HepMfifo writer");
    if(mkfifo(GetOutputFileName().c_str(), 0666) < 0) {
      if(errno != EEXIST) {
        JSWARN << "mkfifo failed,";
        exit(-1);
      }
    }

  };
  virtual ~JetScapeWriterHepMCfifo();

  void Init();
  void Exec();

  bool GetStatus() { return failed(); }
  void Close() { close(); }
 
  // void Open() { open(); }
  // // NEVER use this!
  // // Can work with only one writer, but with a second one it gets called twice
  // void WriteTask(weak_ptr<JetScapeWriter> w);

  // overload write functions
  void WriteEvent();

  // At parton level, we should never accept anything other than a full shower
  // void Write(weak_ptr<Vertex> v);
  void Write(weak_ptr<PartonShower> ps);
  void Write(weak_ptr<Hadron> h);
  void WriteHeaderToFile();

private:
  HepMC3::GenEvent evt;
  vector<HepMC3::GenVertexPtr> vertices;
  HepMC3::GenVertexPtr hadronizationvertex;
  // static RegisterJetScapeModule<JetScapeWriterHepMCfifo> reg;
  /// WriteEvent needs to know whether it should overwrite final partons status to 1
  bool hashadrons=false; 
  
  inline HepMC3::GenVertexPtr
  castVtxToHepMC(const shared_ptr<Vertex> vtx) const {
    double x = vtx->x_in().x();
    double y = vtx->x_in().y();
    double z = vtx->x_in().z();
    double t = vtx->x_in().t();
    HepMC3::FourVector vtxPosition(x, y, z, t);
    // if ( t< 1e-6 ) t = 1e-6; // could do this. Exact 0 is bit quirky but works for hepmc
    return make_shared<GenVertex>(vtxPosition);
  }

  inline HepMC3::GenParticlePtr
  castPartonToHepMC(const shared_ptr<Parton> pparticle) const {
    return castPartonToHepMC(*pparticle);
  }

  inline HepMC3::GenParticlePtr
  castPartonToHepMC(const Parton &particle) const {
    HepMC3::FourVector pmom(particle.px(), particle.py(), particle.pz(),
                            particle.e());
    return make_shared<GenParticle>(pmom, particle.pid(), particle.pstat());
  }

  inline HepMC3::GenParticlePtr
  castHadronToHepMC(const shared_ptr<Hadron> pparticle) const {
    return castHadronToHepMC(*pparticle);
  }

  inline HepMC3::GenParticlePtr
  castHadronToHepMC(const Hadron &particle) const {
    HepMC3::FourVector pmom(particle.px(), particle.py(), particle.pz(),
                            particle.e());
    return make_shared<GenParticle>(pmom, particle.pid(), particle.pstat());
  }

  //int m_precision; //!< Output precision
};

} // end namespace Jetscape

#endif