#ifndef MAPWARETILER_
#define MAPWARETILER_

#include <string.h>
#include <ifexec.h>

namespace MapWareTiling
{
   //for setting the image type (probably don't need to wrap this...)
   enum ImageType {GIF, EPS, AIEPS, PNG, WBMP, PNG24, JPEG, XGIF, BMP, RAW, BMP24};

   //for holding information about a tile with injected pois
   struct MetaTile
   {
      public:
         //image data in bytes
         std::vector<char> imageData;
         //meta data in json format
         std::string metaData;
         //image data type
         ImageType imageType;
         //messages when retrieving the image
         std::string failureMessages;
   };

   //converts ImageType enum to original CMQContentType
   long GetCMQImageType(const ImageType& imageType);

   class MapWareTiler
   {
      public:
         //which server will we be injecting data into
         MapWareTiler(const std::string& server, const short& port, const std::string& clientID, const std::string& path = "mq");
         ~MapWareTiler();

         //set the style name
         void SetStyleName(const std::string& styleName);

         //set the tile height/width scale and center lat,lng
         void SetMapState(const int& width, const int& height, const int& scale,
            const double& centerLat, const double& centerLng,
            const std::string& projection = "Proj:Mercator", const int& dpi=72);

         //add style string and dt to identify it, this should reflect the type of poi to be injected
         //transit example: 3072, "ATPPriority 100 ATPCheckCollision True ATPMarkCollision True Point TextPos Default Symbol Type Vector Name \"Station-train.svg\" Declutter DeclutterFlag On UserDraw False LeaderLinePen Size 28 Color 255,0,0 "
         void AddStyleString(const int& dt, const std::string& styleString);

         //add a new poi to the set of pois that will be injected
         //returns false if the poi was not added to the list
         void AddPOI(const int& dt, const std::string& name, const std::string& id,
            const double& centerLat, const double& centerLng);

         //remove the pois that were previously added
         void ClearPOIs();

         //remove the style strings that were previously added
         void ClearStyleStrings();

         //inject the set of pois into the map, returns a meta tile object
         MetaTile GetTile(const ImageType& imageType, const bool& returnMetaData);

         //clear the session, pois, styles and map state
         void Clear();

      private:
         //makes the call to the server to inject the pois
         CMQExec mapClient;
         //holds all the following objects
         CMQSession* session;
         //sets image size, map scale, center lat,lng
         CMQMapState* mapState;
         //styles to use when drawing the pois
         CMQCoverageStyle* styles;
         //for setting which style/theme you want the tiles to be drawn in
         CMQAutoMapCovSwitch* coverageStyle;
         //holds the list of pois
         CMQFeatureCollection* features;
   };
}

#endif
