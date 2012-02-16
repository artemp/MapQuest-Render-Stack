/*------------------------------------------------------------------------------
 *
 *  Asynchronous tile renderering queue backend factory implementation.
 *
 *  Author: matt.amos@mapquest.com
 *
 *  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *-----------------------------------------------------------------------------*/

#include "backend.hpp"
#include <mapnik/utils.hpp>
#include <boost/optional.hpp>
#include <boost/format.hpp>

namespace dqueue {

using std::string;
using std::map;
using std::runtime_error;
using boost::optional;
namespace pt = boost::property_tree;

class backend_factory 
   : public mapnik::singleton<backend_factory, mapnik::CreateStatic>,
     private boost::noncopyable
{
public:
   friend class mapnik::CreateStatic<backend_factory>;

   bool add(const string &type, 
            runner_creator runner_factory_func, 
            supervisor_creator supervisor_factory_func)
   {
      map<string,backend_creators>::iterator itr = m_creators.find(type);
      if (itr == m_creators.end()) 
      {
         struct backend_creators creators = { runner_factory_func, supervisor_factory_func };
         m_creators.insert(make_pair(type, creators));
         return true;
      }
      return false;
   }

   bool remove(const string &type) 
   {
      map<string,backend_creators>::iterator itr = m_creators.find(type);
      if (itr != m_creators.end()) 
      {
         m_creators.erase(itr);
         return true;
      }
      return false;
   }

   runner_backend *create_runner(const pt::ptree &pt, zmq::context_t &ctx)
   {
      backend_creators create = get_creators(pt);
      return create.runner(pt, ctx);
   }

   supervisor_backend *create_supervisor(const pt::ptree &pt)
   {
      backend_creators create = get_creators(pt);
      return create.supervisor(pt);
   }

private:
   struct backend_creators 
   {
      runner_creator runner;
      supervisor_creator supervisor;
   };

   backend_creators get_creators(const pt::ptree &pt) 
   {
      optional<string> type = pt.get_optional<string>("backend.type");
      if (type) 
      {
         map<string,backend_creators>::iterator itr = m_creators.find(type.get());
         if (itr != m_creators.end())
         {
            return itr->second;
         }
         else
         {
            throw runtime_error((boost::format("Backend type '%1%' not registered.") % type).str());
         }
      }

      throw runtime_error("Key 'type' not found in backend configuration.");
   }

   map<string,backend_creators> m_creators;
};

bool 
register_backend(const string &type, 
                 runner_creator runner, 
                 supervisor_creator supervisor) 
{
   return backend_factory::instance()->add(type, runner, supervisor);
}

runner_backend *
create_runner(const pt::ptree &pt, zmq::context_t &ctx) 
{
   return backend_factory::instance()->create_runner(pt, ctx);
}

supervisor_backend *
create_supervisor(const pt::ptree &pt) 
{
   return backend_factory::instance()->create_supervisor(pt);
}

}


