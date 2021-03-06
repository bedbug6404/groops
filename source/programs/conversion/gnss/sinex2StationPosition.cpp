/***********************************************/
/**
* @file sinex2StationPosition.cpp
*
* @brief Convert GNSS station positions from SINEX to InstrumentVector3d.
*
* @author Sebastian Strasser
* @date 2016-12-07
*/
/***********************************************/

// Latex documentation
#define DOCSTRING docstring
static const char *docstring = R"(
Extracts station positions from \config{inputfileSinex}
(\href{http://www.iers.org/IERS/EN/Organization/AnalysisCoordinator/SinexFormat/sinex.html}{SINEX format description})
and writes an Instrument file \configFile{outputfileInstrument}{instrument} of type VECTOR3D
for each station. Stations can be limited via \config{stationName}, otherwise all stations in \config{inputfileSinex} will be used.
If \config{timeSeries} is provided, positions will be computed at these epochs based on velocity, otherwise reference epoch is used.
If \config{extrapolateBackward} or \config{extrapolateForward} are provided, positions will also be computed for epochs
before the first interval/after the last interval, based on the position and velocity of the first/last interval.
If \config{inputfileDiscontinuities} is provided (one file per station, create with \program{Sinex2StationDiscontinuities}),
position extrapolation will stop at the first discontinuity before the first interval/after the last interval.

See also \program{Sinex2StationPostSeismicDeformation}.
)";

/***********************************************/

#include "programs/program.h"
#include "base/string.h"
#include "classes/timeSeries/timeSeries.h"
#include "inputOutput/fileSinex.h"
#include "files/fileInstrument.h"

/***** CLASS ***********************************/

/** @brief Convert GNSS station positions from SINEX to InstrumentVector3d.
* @ingroup programsConversionGroup */
class Sinex2StationPosition
{
  class Interval
  {
  public:
    std::string solutionId;
    Time referenceTime;
    Time timeStart;
    Time timeEnd;
    Vector3d position;
    Vector3d velocity;

    bool operator<(const Interval &other) { return timeStart < other.timeStart; }
  };

  class Station
  {
  public:
    std::vector<Interval> intervals;
    std::vector<Time> discontinuities;

    Vector3dArc arc(const std::vector<Time> &times, Bool extrapolateForward, Bool extrapolateBackward);
  };

public:
  void run(Config &config);
};

GROOPS_REGISTER_PROGRAM(Sinex2StationPosition, SINGLEPROCESS, "Convert GNSS station positions from SINEX to InstrumentVector3d.", Conversion, Gnss, Instrument)
GROOPS_RENAMED_PROGRAM(GnssSinex2StationPosition, Sinex2StationPosition, date2time(2018, 12, 4))

/***********************************************/

Vector3dArc Sinex2StationPosition::Station::arc(const std::vector<Time> &times, Bool extrapolateForward, Bool extrapolateBackward)
{
  try
  {
    Vector3dArc arc;
    if(!intervals.size() || !times.size())
      return arc;
    if(!discontinuities.size())
      discontinuities = {Time(), date2time(2500, 1, 1)};

    // expand first/last interval in case of extrapolation so a simple interval search can be done afterwards for each epoch
    if(extrapolateBackward && times.front() < intervals.front().timeStart)
      intervals.front().timeStart = std::max(times.front(), *std::upper_bound(discontinuities.begin(), discontinuities.end(), intervals.front().timeStart));
    if(extrapolateForward && times.back() >= intervals.back().timeEnd)
      intervals.back().timeEnd = std::min(times.back()+seconds2time(1), *std::lower_bound(discontinuities.begin(), discontinuities.end(), intervals.back().timeEnd));

    UInt idInterval = 0;
    for(const auto &time : times)
    {
      while(idInterval+1 < intervals.size() && time >= intervals.at(idInterval+1).timeStart)
        idInterval++;

      if(time < intervals.at(idInterval).timeStart || time >= intervals.at(idInterval).timeEnd)
        continue; // epoch is outside interval

      Vector3dEpoch epoch;
      epoch.time = time;
      epoch.vector3d = intervals.at(idInterval).position + intervals.at(idInterval).velocity * (time-intervals.at(idInterval).referenceTime).mjd()/365.25;
      arc.push_back(epoch);
    }

    return arc;
  }
  catch(std::exception &e)
  {
    GROOPS_RETHROW(e)
  }
}

/***********************************************/

void Sinex2StationPosition::run(Config &config)
{
  try
  {
    FileName fileNameInstrument, fileNameDiscontinuities, fileNameSinex;
    std::vector<std::string> stationNames;
    std::string variableLoopStation;
    TimeSeriesPtr timeSeries;
    Bool extrapolateForward, extrapolateBackward;

    readConfig(config, "outputfileInstrument",     fileNameInstrument,      Config::MUSTSET,   "",        "loop variable is replaced with station name (e.g. wtzz)");
    readConfig(config, "inputfileSinex",           fileNameSinex,           Config::MUSTSET,   "",        "SINEX file (.snx or .ssc)");
    readConfig(config, "inputfileDiscontinuities", fileNameDiscontinuities, Config::OPTIONAL,  "",        "discontinuities file per station; loop variable is replaced with station name (e.g. wtzz)");
    readConfig(config, "variableLoopStation",      variableLoopStation,     Config::DEFAULT,   "station", "variable name for station loop");
    readConfig(config, "stationName",              stationNames,            Config::OPTIONAL,  "",        "convert only these stations");
    readConfig(config, "timeSeries",               timeSeries,              Config::DEFAULT,   "",        "compute positions for these epochs based on velocity");
    readConfig(config, "extrapolateForward",       extrapolateForward,      Config::DEFAULT,   "0",       "also compute positions for epochs after last interval defined in SINEX file");
    readConfig(config, "extrapolateBackward",      extrapolateBackward,     Config::DEFAULT,   "0",       "also compute positions for epochs before first interval defined in SINEX file");
    if(isCreateSchema(config)) return;

    logStatus << "read SINEX file <" << fileNameSinex << ">" << Log::endl;
    std::map<std::string, Station> stations;
    Sinex sinex(fileNameSinex);
    std::vector<Sinex::Epoch> epochs = sinex.getBlock<Sinex::SinexSolutionEpochs>("SOLUTION/EPOCHS")->epochs();
    for(const auto &epoch : epochs)
    {
      std::string name = String::lowerCase(epoch.siteCode);
      if(stationNames.size() && std::find(stationNames.begin(), stationNames.end(), name) == stationNames.end())
        continue;

      Interval interval;
      interval.solutionId = epoch.solutionId;
      interval.timeStart  = epoch.timeStart;
      interval.timeEnd    = epoch.timeEnd;
      stations[name].intervals.push_back(interval);
    }
    for(auto &&station : stations)
      std::sort(station.second.intervals.begin(), station.second.intervals.end());

    std::vector<Sinex::Parameter> parameters = sinex.getBlock<Sinex::SinexSolutionVector>("SOLUTION/ESTIMATE")->parameters();
    for(const auto &param : parameters)
    {
      std::string name = String::lowerCase(param.siteCode);
      if((stationNames.size() && std::find(stationNames.begin(), stationNames.end(), name) == stationNames.end()) ||
         (param.parameterType != "STAX" && param.parameterType != "STAY" && param.parameterType != "STAZ" &&
          param.parameterType != "VELX" && param.parameterType != "VELY" && param.parameterType != "VELZ"))
        continue;

      auto interval = std::find_if(stations[name].intervals.begin(), stations[name].intervals.end(), [&](const Interval &i){ return i.solutionId == param.solutionId; });
      if(interval == stations[name].intervals.end())
        throw(Exception(name + ": interval for solutionId " + param.solutionId + " not found"));

      interval->referenceTime  = param.time;
      if(     param.parameterType == "STAX")
        interval->position.x() = param.value;
      else if(param.parameterType == "STAY")
        interval->position.y() = param.value;
      else if(param.parameterType == "STAZ")
        interval->position.z() = param.value;
      else if(param.parameterType == "VELX")
        interval->velocity.x() = param.value;
      else if(param.parameterType == "VELY")
        interval->velocity.y() = param.value;
      else if(param.parameterType == "VELZ")
        interval->velocity.z() = param.value;
    }

    VariableList fileNameVariableList;
    addVariable(variableLoopStation, fileNameVariableList);
    if(!fileNameDiscontinuities.empty())
    {
      logStatus << "read discontinuity files <" << fileNameDiscontinuities << ">" << Log::endl;
      for(const auto &name : stationNames)
      {
        fileNameVariableList[variableLoopStation]->setValue(name);
        try
        {
          if(stations.find(name) != stations.end())
            stations[name].discontinuities = InstrumentFile::read(fileNameDiscontinuities(fileNameVariableList)).times();
        }
        catch(...)
        {
          logWarning << name << ": discontinuity file not found" << Log::endl;
        }
      }
    }

    if(stations.size())
    {
      logStatus << "write instrument files <" << fileNameInstrument << ">" << Log::endl;
      std::vector<Time> times = timeSeries->times();
      for(auto &&station : stations)
      {
        Vector3dArc arc;
        if(!times.size() && station.second.intervals.size()) // no times given ==> use reference time of last interval
          arc = station.second.arc({station.second.intervals.back().referenceTime}, extrapolateForward, extrapolateBackward);
        else
          arc = station.second.arc(times, extrapolateForward, extrapolateBackward);
        fileNameVariableList[variableLoopStation]->setValue(station.first);
        InstrumentFile::write(fileNameInstrument(fileNameVariableList), arc);
      }
    }
    else
      logInfo << "no stations found" << Log::endl;
  }
  catch(std::exception &e)
  {
    GROOPS_RETHROW(e)
  }
}

/***********************************************/
