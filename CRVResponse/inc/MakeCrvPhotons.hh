#ifndef MakeCrvPhotons_h
#define MakeCrvPhotons_h

#include <vector>
#include <map>
#include "CLHEP/Vector/ThreeVector.h"
#include "CLHEP/Random/Randomize.h"

#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>

namespace mu2eCrv
{

struct LookupConstants
{
  int    version1, version2;
  double halfThickness, halfWidth, halfLength;
  double speedOfLightFiber;
  double rindexScintillator;
  double rindexFiber;
  double cerenkovEnergyIntervalScintillator;
  double cerenkovEnergyIntervalFiber;
  double fiberSeparation, holeRadiusX, holeRadiusY, fiberRadius, scintillatorCornerRadius;
  void Write(const std::string &filename);
  void Read(std::ifstream &lookupfile);
};

struct LookupBinDefinitions
{
  std::vector<double> xBins;
  std::vector<double> yBins;
  std::vector<double> zBins;
  std::vector<double> betaBins;
  std::vector<double> thetaBins;
  std::vector<double> phiBins;
  std::vector<double> rBins;
  void WriteVector(std::vector<double> &v, std::ofstream &o);
  void ReadVector(std::vector<double> &v, std::ifstream &i);
  void Write(const std::string &filename);
  void Read(std::ifstream &lookupfile);

  unsigned int getNScintillatorBins();
  unsigned int getNFiberBins();

  unsigned int findBin(const std::vector<double> &v, const double &x, bool &notFound);
  int findScintillatorBin(double x, double y, double z);
  int findFiberBin(double beta, double theta, double phi, double r, double z);
  bool findScintillatorBinReverse(unsigned int bin, double &xbin, double &ybin, double &zbin);
  bool findFiberBinReverse(unsigned int bin, double &betabin, double &thetabin, double &phibin, double &rbin, double &zbin);
};

struct LookupBin
{
  static const int nTimeDelays=150;
  static const int nFiberEmissions=15;
  static const unsigned short probabilityScale=10000;  //still within unsigned short (2 bytes)
  //the lookup tables encodes probabilities as probability*probabilityScale(10000), 
  //so that the probabilities can be stored as integers.
  //therefore, the probability of 1 is stored as 10000.

  float arrivalProbability[4];
  unsigned short timeDelays[4][nTimeDelays];
  unsigned short fiberEmissions[4][nFiberEmissions];
  void Write(const std::string &filename);
  void Read(std::ifstream &lookupfile);
};



class MakeCrvPhotons
{
  public:

    MakeCrvPhotons(CLHEP::RandFlat &randFlat, CLHEP::RandGaussQ &randGaussQ, CLHEP::RandPoissonQ &randPoissonQ) : 
                                                      _randFlat(randFlat), _randGaussQ(randGaussQ), _randPoissonQ(randPoissonQ) {}

    ~MakeCrvPhotons();

    const std::string         &GetFileName() const {return _fileName;}

    void                      LoadLookupTable(const std::string &filename);
    void                      LoadVisibleEnergyAdjustmentTable(const std::string &filename);
    void                      MakePhotons(const CLHEP::Hep3Vector &stepStart,   //they need to be points
                                      const CLHEP::Hep3Vector &stepEnd,         //local to the CRV bar
                                      double timeStart, double timeEnd,
                                      int PDGcode, double beta, double charge,
                                      double energyDepositedTotal,
                                      double energyDepositedNonIonizing,
                                      double trueStepLength,
                                      double scintillationYieldAdjustment=0);   //allows small random variations of the scintillation yield for individual counters
    int                       GetNumberOfPhotons(int SiPM);
    const std::vector<double> &GetArrivalTimes(int SiPM);
    void                      SetScintillationYield(double yield) {_scintillationYield=yield;}
    void                      SetScintillatorBirksConstant(double birksConstant) {_scintillatorBirksConstant=birksConstant;}
    void                      SetScintillatorRatioFastSlow(double ratio) {_scintillatorRatioFastSlow=ratio;}
    void                      SetScintillatorDecayTimeFast(double decayTime) {_scintillatorDecayTimeFast=decayTime;}
    void                      SetScintillatorDecayTimeSlow(double decayTime) {_scintillatorDecayTimeSlow=decayTime;}
    void                      SetFiberDecayTime(double decayTime) {_fiberDecayTime=decayTime;}

  private:

    std::string               _fileName;

    std::vector<double>       _arrivalTimes[4];
    double                    _scintillationYield;
    double                    _scintillatorBirksConstant;
    double                    _scintillatorRatioFastSlow;
    double                    _scintillatorDecayTimeFast; 
    double                    _scintillatorDecayTimeSlow; 
    double                    _fiberDecayTime;

    LookupConstants           _LC;
    LookupBinDefinitions      _LBD;
    std::vector<LookupBin>    _bins[4];

    CLHEP::RandFlat           &_randFlat;
    CLHEP::RandGaussQ         &_randGaussQ;
    CLHEP::RandPoissonQ       &_randPoissonQ;

    bool   IsInsideScintillator(const CLHEP::Hep3Vector &p);
    int    IsInsideFiber(const CLHEP::Hep3Vector &p, double &r);
    double GetRandomTime(const LookupBin *theBin, int SiPM, bool &overflow);
    int    GetRandomFiberEmissions(const LookupBin *theBin, int SiPM, bool &overflow);
    double GetAverageNumberOfCerenkovPhotons(double beta, double charge, double rindex, double cerenkovEnergyInterval);
    int    GetNumberOfPhotonsFromAverage(double average, int nSteps);

    double VisibleEnergyDeposition(int PDGcode, double stepLength,
                                   double energyDepositedTotal,
                                   double energyDepositedNonIonizing);
    double FindVisibleEnergyAdjustmentFactor(double energy);

    std::map<double,double>    _visibleEnergyAdjustmentTable;

    public:
    void   DrawHistograms();
};

} //namespace mu2e

#endif