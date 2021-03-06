/***********************************************/
/**
* @file fileTideGeneratingPotential.cpp
*
* @brief Read/write tide genearting potential.
*
* @author Torsten Mayer-Guerr
* @date 2005-01-14
*
*/
/***********************************************/

#define DOCSTRING_FILEFORMAT_TideGeneratingPotential

#include "base/import.h"
#include "base/tideGeneratingPotential.h"
#include "inputOutput/fileArchive.h"
#include "files/fileFormatRegister.h"
#include "files/fileTideGeneratingPotential.h"

GROOPS_REGISTER_FILEFORMAT(TideGeneratingPotential, FILE_TIDEGENERATINGPOTENTIAL_TYPE)

/***********************************************/

template<> void save(OutArchive &ar, const TideGeneratingPotential &x)
{
  const UInt constituentsCount = x.size();
  ar<<nameValue("constituentsCount", constituentsCount);
  ar.comment("Dood.   cos                       sin                      name");
  ar.comment("===============================================================");
  for(UInt i=0; i<constituentsCount; i++)
  {
    std::string name = (x.at(i).name() != x.at(i).code()) ? x.at(i).name() : "";
    ar<<beginGroup("constituent");
    ar<<nameValue("doodson", *dynamic_cast<const Doodson*>(&x.at(i)));
    ar<<nameValue("c",       x.at(i).c);
    ar<<nameValue("s",       x.at(i).s);
    ar<<nameValue("name",    name);
    ar<<endGroup("constituent");
  }
}

/***********************************************/

template<> void load(InArchive &ar, TideGeneratingPotential &x)
{
  UInt constituentsCount;
  ar>>nameValue("constituentsCount", constituentsCount);
  x.resize(constituentsCount);
  for(UInt i=0; i<constituentsCount; i++)
  {
    ar>>beginGroup("constituent");
    Doodson     dood;
    Double      c, s;
    std::string name;
    ar>>nameValue("doodson", dood);
    ar>>nameValue("c",       c);
    ar>>nameValue("s",       s);
    ar>>nameValue("name",    name);
    x.at(i) = TideGeneratingConstituent(dood, c, s);
    ar>>endGroup("constituent");
  }
}

/***********************************************/

void writeFileTideGeneratingPotential(const FileName &fileName, const TideGeneratingPotential &x)
{
  try
  {
    OutFileArchive file(fileName, FILE_TIDEGENERATINGPOTENTIAL_TYPE);
    file<<nameValue("tideGeneratingPotential", x);
  }
  catch(std::exception &e)
  {
    GROOPS_RETHROW(e)
  }
}

/***********************************************/

void readFileTideGeneratingPotential(const FileName &fileName, TideGeneratingPotential &x)
{
  try
  {
    InFileArchive file(fileName, FILE_TIDEGENERATINGPOTENTIAL_TYPE);
    file>>nameValue("tideGeneratingPotential", x);
  }
  catch(std::exception &e)
  {
    GROOPS_RETHROW(e)
  }
}

/***********************************************/
