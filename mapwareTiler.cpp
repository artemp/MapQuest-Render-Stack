#include "mapwareTiler.hpp"
#include "logging/logger.hpp"

namespace MapWareTiling
{
   long GetCMQImageType(const ImageType& imageType)
   {
      switch(imageType)
      {
         case GIF:
            return CMQContentType::GIF;
         case EPS:
            return CMQContentType::EPS;
         case AIEPS:
            return CMQContentType::AIEPS;
         case PNG:
            return CMQContentType::PNG;
         case WBMP:
            return CMQContentType::WBMP;
         case PNG24:
            return CMQContentType::PNG24;
         case JPEG:
            return CMQContentType::JPEG;
         case XGIF:
            return 77;
         case BMP:
            return 99;
         case BMP24:
            return 97;
         default:
         case RAW:
            return 98;
      }
   }

   MapWareTiler::MapWareTiler(const std::string& server, const short& port, const std::string& clientID,
      const std::string& path):session(NULL), mapState(NULL), styles(NULL), coverageStyle(NULL), features(NULL)
   {
      //need this to make the call to the map server
      this->mapClient.SetServerName(server.c_str());
      this->mapClient.SetServerPath(path.c_str());
      this->mapClient.SetServerPort(port);
      this->mapClient.SetClientId(clientID.c_str());

      //setup the session
      this->Clear();
   }

   MapWareTiler::~MapWareTiler()
   {
      //the session destructor calls the destructor of each attached object
      if(this->session != NULL)
         delete this->session;
   }

   void MapWareTiler::SetStyleName(const std::string& styleName)
   {
      //keep this lets the switcher determine what to use at what zoom levels
      this->coverageStyle->SetName("mqmauto");
      //set the style we want to use
      this->coverageStyle->SetStyle(styleName.c_str());
   }

   void MapWareTiler::SetMapState(const int& width, const int& height, const int& scale,
      const double& centerLat, const double& centerLng, const std::string& projection, const int& dpi)
   {
      // Use mercator projection by specifying the mapname
      this->mapState->SetMapName(projection.c_str());

      // Define the width of the map in pixels
      this->mapState->SetWidthPixels(width, dpi);

      // Define the height of the map in pixels
      this->mapState->SetHeightPixels(height, dpi);

      // The MapScale property tells the server the scale at which to display the map
      // Level of detail displayed varies depending on the scale of the map
      this->mapState->SetMapScale(scale);

      // Specify the latitude/longitude coordinate to center the map
      this->mapState->SetCenter(CMQLatLng(centerLat, centerLng));
   }

   void MapWareTiler::AddStyleString(const int& dt, const std::string& styleString)
   {
      // A DTStyle object is an object that contains graphical information about a
      // point to display on the map.  This information includes, but is not limited to,
      // the symbol for a point, whether to label the point, and if so, the font to use.
      CMQDTStyleEx* pointDTStyle = new CMQDTStyleEx();

      // Set a style string for this
      pointDTStyle->SetDT(dt);
      pointDTStyle->SetStyleString(styleString.c_str());

      // The CoverageStyle object contains user-defined DTStyle (Display Type) objects, which can
      // override default styles set in the style pool.
      // This adds a DTStyle object to the CoverageStyle object.
      this->styles->Add(pointDTStyle);
   }

   void MapWareTiler::AddPOI(const int& dt, const std::string& name, const std::string& id,
      const double& centerLat, const double& centerLng)
   {
      // A PointFeature object contains information about where to display a
      // POI (Point of Interest) on a map, as well information about the point, such as the
      // distance from the center in a radius search.
      CMQPointFeature* feature = new CMQPointFeature();

      // This property must coincide with the DT of the DTStyle object used
      // in determining the display characteristics of this PointFeature.
      feature->SetDT(dt);

      // Set the user key
      feature->SetKey(id.c_str());

      // When a DTStyle object's LabelVisible property is set to true, the Name property
      // is displayed as the label.
      feature->SetName(name.c_str());

      // The CenterLatLng object contains the latitude/longitude coordinate
      // used to determine where to display the point on a map.
      feature->SetCenterLatLng(CMQLatLng(centerLat, centerLng));

      // This example adds a PointFeature to a FeatureCollection.  The Features in the
      // collection will be added to the map that is returned to the end user.
      this->features->Add(feature);
   }

   //inject the set of pois into the map, returns false and failure message if it fails
   MetaTile MapWareTiler::GetTile(const ImageType& imageType, const bool& returnMetaData)
   {
      //make a tile
      MetaTile tile;
      tile.failureMessages = "";

      // The DisplayState determines what type of image is returned
      CMQDisplayState displayState;
      displayState.SetContentType(GetCMQImageType(imageType));
      // Return the meta data with the tile
      displayState.SetMetaInjection(returnMetaData);

      // This call generates the actual GIF image resulting from the given Session Object.
      CMQIOMem image;
      try
      {
         this->mapClient.GetMapImageDirectEx(session, image, &displayState);
      }
      catch (CMQException& e)
      {
         tile.failureMessages += (const char*)e.GetExceptionString();
         return tile;
      }

      //if there isn't any image data
      if(image.size() < 1)
      {
         tile.failureMessages += "No image was returned";
         return tile;
      }

      //where the image stops
      size_t position = image.size();
      //if we need to peel out the meta data
      if(returnMetaData)
      {
         //figure out where the meta data begins (dumb parsing)
         long opens = 0, closes = 0;
         while(position > 0 && ((opens == 0 && closes == 0) || opens != closes))
         {
            position--;
            if(image.buffer()[position] == '}')
               closes++;
            else if(image.buffer()[position] == '{')
               opens++;
         }

         //couldn't properly find the meta data
         if(position < 1 || opens != closes  || opens == 0 || closes == 0)
         {
            tile.failureMessages += "No meta data found within the tile";
            return tile;
         }//save the meta data
         else
            tile.metaData = &(image.buffer()[position]);
      }

      //save the image data
      tile.imageData.resize(position);
      memcpy(&tile.imageData[0], image.buffer(), position);
      tile.imageType = imageType;

      //we're done
      return tile;      
   }

   void MapWareTiler::ClearPOIs()
   {
      //this calls the destructor of each feature
      this->features->DeleteAll();
   }

   //remove the style strings that were previously added
   void MapWareTiler::ClearStyleStrings()
   {
      this->styles->DeleteAll();
   }

   void MapWareTiler::Clear()
   {
      //the session destructor calls the destructor of each attached object
      if(this->session != NULL)
         delete this->session;

      //add everything into the session that we could be using
      this->session = new CMQSession();
      this->mapState = new CMQMapState();
      this->styles = new CMQCoverageStyle();
      this->coverageStyle = new CMQAutoMapCovSwitch();
      this->features = new CMQFeatureCollection();

      //require a map state (to know which tile we want)
      session->AddOne(this->mapState);
      //which coverage style do we want
      session->AddOne(this->coverageStyle);
      //add the poi styles
      session->AddOne(this->styles);
      //add the pois
      session->AddOne(this->features);
   }
}
