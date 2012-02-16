#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/optional.hpp>

#include "mapwareTiler.hpp"

using namespace boost::python;
using namespace MapWareTiling;

//output 'raw' pixels from the image data vector
PyObject* tostring(const MetaTile &tile)
{
   //return a boost python string from the vector of bytes
   if(tile.imageData.size() > 0)
      return PyString_FromStringAndSize((const char*)&tile.imageData[0], tile.imageData.size());
   else
      return PyString_FromStringAndSize(NULL, 0);
}

BOOST_PYTHON_MODULE(mapwareTiler)
{
   //enum of different tile image types
   enum_<MapWareTiling::ImageType>("ImageType")
      .value("GIF", MapWareTiling::GIF)
      .value("EPS", MapWareTiling::EPS)
      .value("AIEPS", MapWareTiling::AIEPS)
      .value("PNG", MapWareTiling::PNG)
      .value("WBMP", MapWareTiling::WBMP)
      .value("PNG24", MapWareTiling::PNG24)
      .value("JPEG", MapWareTiling::JPEG)
      .value("XGIF", MapWareTiling::XGIF)
      .value("BMP", MapWareTiling::BMP)
      .value("BMP24", MapWareTiling::BMP24)
      .value("RAW", MapWareTiling::RAW)
      ;

   //wrap the stl vector that will hold the image data
   boost::python::class_<std::vector<char> >("ImageBytes")
      .def(boost::python::vector_indexing_suite<std::vector<char> >())
      ;

   //container for holding the returned tile and meta data
   class_<MapWareTiling::MetaTile>("MetaTile")
      .def_readwrite("imageData", &MetaTile::imageData)
      .def_readwrite("metaData", &MetaTile::metaData)
      .def_readwrite("imageType", &MetaTile::imageType)
      .def_readwrite("failureMessages", &MetaTile::failureMessages)
      .def("tostring", &tostring)
      ;

   //class which does the actual tile fetching
   class_<MapWareTiling::MapWareTiler>("MapWareTiler", init<std::string, short, std::string, optional<std::string> >())
      .def("SetStyleName", &MapWareTiler::SetStyleName)
      .def("SetMapState", &MapWareTiler::SetMapState)
      .def("AddStyleString", &MapWareTiler::AddStyleString)
      .def("AddPOI", &MapWareTiler::AddPOI)
      .def("ClearPOIs", &MapWareTiler::ClearPOIs)
      .def("ClearStyleStrings", &MapWareTiler::ClearStyleStrings)
      .def("GetTile", &MapWareTiler::GetTile)
      .def("Clear", &MapWareTiler::Clear)
      ;
}
