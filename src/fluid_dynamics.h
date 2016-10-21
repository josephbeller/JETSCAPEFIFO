// Copyright JETSCAPE Collaboration @ 2016
// This is a general basic class for hydrodynamics
// This is written by Longgang Pang and Chun Shen

#ifndef SRC_FLUID_DYNAMICS_H_
#define SRC_FLUID_DYNAMICS_H_

#include <vector>
#include <cstring>

#include "realtype.h"

//overload +-*/ for easier linear interpolation
class FluidCellInfo {
 public:
    // data structure for outputing fluid cell information
    real energy_density;    // local energy density [GeV/fm^3]
    real entropy_density;   // local entropy density [1/fm^3]
    real temperature;       // local temperature [GeV]
    real pressure;          // thermal pressure [GeV/fm^3]
    real qgp_fraction;
    real mu_B;              // net baryon chemical potential [GeV]
    real mu_C;              // net charge chemical potential [GeV]
    real mu_S;              // net strangeness chemical potential [GeV]
    real vx, vy, vz;        // flow velocity
    real pi[4][4];          // shear stress tensor [GeV/fm^3]
    real bulk_Pi;           // bulk viscous pressure [GeV/fm^3]

    FluidCellInfo() = default;    

    FluidCellInfo inline operator*=(real b);
};

/// adds \f$ c = a + b \f$
inline FluidCellInfo operator+(FluidCellInfo a, const FluidCellInfo & b)
{
    a.energy_density += b.energy_density;
    a.entropy_density += b.entropy_density;
    a.temperature += b.temperature;
    a.pressure += b.pressure;
    a.qgp_fraction += b.qgp_fraction;
    a.mu_B += b.mu_B;
    a.mu_C += b.mu_C;
    a.mu_S += b.mu_S;
    a.vx += b.vx;
    a.vy += b.vy;
    a.vz += b.vz;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            a.pi[i][j] += b.pi[i][j];
        }
    }
    a.bulk_Pi += b.bulk_Pi;
    return a;
}

/// multiply the fluid cell with a scalar factor
FluidCellInfo inline FluidCellInfo::operator*=(real b){
    this->energy_density *= b;
    this->entropy_density *= b;
    this->temperature *= b;
    this->pressure *= b;
    this->qgp_fraction *= b;
    this->mu_B *= b;
    this->mu_C *= b;
    this->mu_S *= b;
    this->vx *= b;
    this->vy *= b;
    this->vz *= b;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            this->pi[i][j] *= b;
        }
    }
    this->bulk_Pi *= b;
    return *this;
}

/// multiply \f$ c = a * b \f$
inline FluidCellInfo operator*(real a, FluidCellInfo b){
    b *= a;
    return b;
}

/// multiply \f$ c = a * b \f$
inline FluidCellInfo operator*(FluidCellInfo a, real b){
    a *= b;
    return a;
}

/// division \f$ c = a / b \f$
inline FluidCellInfo operator/(FluidCellInfo a, real b){
    a *= 1.0/b;
    return a;
}

// print the fluid cell information for debuging
std::ostream &operator<<(std::ostream &os, const FluidCellInfo &cell) {
    os << "energy_density=" << cell.energy_density << std::endl; 
    os << "entropy_density=" << cell.entropy_density << std::endl; 
    os << "temperature=" << cell.temperature << std::endl; 
    os << "pressure=" << cell.pressure << std::endl;
    os << "qgp_fraction=" << cell.qgp_fraction << std::endl;
    os << "mu_B=" << cell.mu_B << std::endl;
    os << "mu_C=" << cell.mu_C << std::endl;
    os << "mu_S=" << cell.mu_S << std::endl;
    os << "vx=" << cell.vx << std::endl;
    os << "vy=" << cell.vy << std::endl;
    os << "vz=" << cell.vz << std::endl;
    os << "pi[mu][nu]=" << std::endl;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            os << cell.pi[i][j] << ' ';
        }
        os << std::endl;
    }
    os << "bulk_Pi=" << cell.bulk_Pi;
    return os << std::endl;
}

typedef struct {
    // data structure for outputing hyper-surface information
    real d3sigma_mu[4];     // surface vector
    real energy_density;    // local energy density [GeV/fm^3]
    real entropy_density;   // local entropy density [1/fm^3]
    real temperature;       // local temperature [GeV]
    real pressure;          // thermal pressure [GeV/fm^3]
    real qgp_fraction;
    real mu_B;              // net baryon chemical potential [GeV]
    real mu_C;              // net charge chemical potential [GeV]
    real mu_S;              // net strangeness chemical potential [GeV]
    real vx, vy, vz;        // flow velocity
    real pi[4][4];          // shear stress tensor [GeV/fm^3]
    real bulk_Pi;           // bulk viscous pressure [GeV/fm^3]
} SurfaceCellInfo;


class Parameter{
 public:
    // hydro parameters
    char* hydro_input_filename;
};


class EvolutionHistory{
 public:
    real tau_min, dtau;
    real x_min, dx;
    real y_min, dy;
    real eta_min, deta;
    int ntau, nx, ny, neta;
    // default: set using_tz_for_tau_eta=true
    bool using_tz_for_tau_eta;
    // the bulk information
    std::vector<FluidCellInfo> data;

    EvolutionHistory();
    ~EvolutionHistory() {data.clear();}
};


class FluidDynamics{
 protected:
    // record hydro start and end proper time [fm/c]
    real hydro_tau_0, hydro_tau_max;
    // record hydro freeze out temperature [GeV]
    real hydro_freeze_out_temperature;

    // record hydro running status
    int hydro_status;  // 0: nothing happened
                       // 1: hydro has been initialized
                       // 2: hydro is evolving
                       // 3: all fluid cells have reached freeze-out
                       // -1: An error occurred
 public:
    FluidDynamics() {};
    ~FluidDynamics() {};

    // How to store this data? In memory or hard disk?
    // 3D hydro may eat out the memory,
    // for large dataset, std::deque is better than std::vector.
    EvolutionHistory bulk_info;

    /*Keep this interface open in the beginning.*/
    // FreezeOutHyperSf hyper_sf;

    /* currently we have no standard for passing configurations */
    // pure virtual function; to be implemented by users
    // should make it easy to save evolution history to bulk_info
    virtual void initialize_hydro(Parameter parameter_list) {};

    virtual void evolve_hydro() {};

    // virtual void evolution(const EnergyMomentumTensor & tmn,
    //                        real xmax, real ymax, real hmax,
    //                        real tau0, real dtau, real dx,
    //                        real dy, real dh, real etaos,
    //                        real dtau_out, real dx_out, real dy_out,
    //                        real dh_out) const = 0;

    // the following functions should be implemented in Jetscape
    int get_hydro_status() {return(hydro_status);}
    real get_hydro_start_time() {return(hydro_tau_0);}
    real get_hydro_end_time() {return(hydro_tau_max);}
    real get_hydro_freeze_out_temperature() {
        return(hydro_freeze_out_temperature);
    }

    // this function retrives hydro information at a given space-tim point
    // the detailed implementation is left to the hydro developper
    void get_hydro_info(real t, real x, real y, real z,
                                FluidCellInfo* fluid_cell_info_ptr) {};

    // this function returns hypersurface for Cooper-Frye or recombination
    // the detailed implementation is left to the hydro developper
    virtual void get_hypersurface(real T_cut,
                                  SurfaceCellInfo* surface_list_ptr) {};

    // all the following functions will call function get_hydro_info()
    // to get thermaldynamic and dynamical information at a space-time point
    // (time, x, y, z)
    real get_energy_density(real time, real x, real y, real z);
    real get_entropy_density(real time, real x, real y, real z);
    real get_temperature(real time, real x, real y, real z);
    real get_qgp_fraction(real time, real x, real y, real z);

    // real3 return std::make_tuple(vx, vy, vz)
    real3 get_3fluid_velocity(real time, real x, real y, real z);
    // real4 return std::make_tuple(ut, ux, uy, uz)
    real4 get_4fluid_velocity(real time, real x, real y, real z);

    real get_net_baryon_density(real time, real x, real y, real z);
    real get_net_charge_density(real time, real x, real y, real z);
};

#endif  // SRC_FLUID_DYNAMICS_H_
