/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: artem@mapnik-consulting.com
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

#ifndef TASK_QUEUE_HPP
#define TASK_QUEUE_HPP

#include "tile_protocol.hpp"
#include "logging/logger.hpp"
// stl
#include <iostream>
#include <vector>
#include <ctime> // for std::time_t
// boost
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/optional.hpp>

namespace rendermq 
{
        
using namespace boost::multi_index;
struct task : tile_protocol
{
   typedef std::vector<std::pair<tile_protocol,std::string> > cont_type;
   typedef cont_type::const_iterator iterator;
    
   explicit task(tile_protocol const& t, int priority=0)
      : tile_protocol(t),
        priority_(priority),
        timestamp_(std::time(0)),
        processed_(false) {}
    
   task(int x,int y, int z, const std::string &style, protoCmd status, protoFmt fmt, std::time_t last_mod_, std::time_t req_last_mod_, int priority=0)
      : tile_protocol(status,x,y,z,0,style,fmt,last_mod_,req_last_mod_),
        priority_(priority),
        timestamp_(std::time(0)),
        processed_(false) {}
    
   void set_priority(int priority)
   {
      priority_ = priority;
   }
   
   int priority() const 
   {
      return priority_;
   }
   
   void add_subscriber(tile_protocol const& tile, std::string const& addr)
   {
      if (&tile == this) {
         LOG_WARNING("Adding self to own subscribers vector - this shouldn't happen");
      }
      subscribers_.push_back(std::make_pair(tile,addr));
   }
   
   std::pair<iterator,iterator> subscribers() const
   {
      return std::make_pair(subscribers_.begin(),subscribers_.end());
   }
   
   void set_processed(bool b) 
   {
      processed_ = b;
   }
   
   bool processed() const
   {
      return processed_;
   }
   
   void set_timestamp(std::time_t const& t)
   {
      timestamp_ = t;
   }
   
   std::time_t timestamp() const
   {
      return timestamp_;
   }
    
   int priority_;
   std::time_t timestamp_;
   cont_type subscribers_;
   bool processed_; 
};

inline bool operator==(task const& t0, task const& t1)
{
   return (t0.x == t1.x &&
           t0.y == t1.y && 
           t0.z == t1.z &&
           t0.style == t1.style);
}
    
inline std::size_t hash_value( task const& t)
{
   return hash_value(static_cast<const tile_protocol &>(t));
}

struct priority {};
struct metatile {};
struct timestamp {};

/* a priority queue of tasks, sorted by priority (highest priority at the 
 * *front* of the queue) and unique by position and style parameters. finally
 * it's sorted on timestamp.
 */
class task_queue 
{
   typedef multi_index_container<task,
                                 indexed_by<
// sort on priority
                                 ordered_non_unique<tag<priority>,
                                                    member<task,int, &task::priority_>,
                                                    std::greater<int> > ,
// hash index on x,y,z & style
                                 hashed_unique<tag<metatile>,
                                               identity<task> >,
// index to order by timestamp 
                                 ordered_non_unique<tag<timestamp>,
                                                    member<task,std::time_t, &task::timestamp_>,
                                                    std::less<int> >
                                 > > cont_type;
    
   typedef cont_type::index<rendermq::priority>::type priority_index_type;
   typedef priority_index_type::iterator priority_index_iterator;

   /* boost multi-index needs all modifications to the entries in the
    * data structure to happen through these functor objects, so that
    * the multi-index container can make the appropriate changes to
    * the index after the modification happens.
    */

   // sets the priority of a task
   struct priority_modifier
   {
      priority_modifier(int priority)
         : priority_(priority) {}
        
      void operator() (task & t)
      {           
         t.set_priority(priority_);
      }
        
      int priority_;
   };
    
   // called when a new task is to be added. this functor may need to
   // adjust the priority of a task and the formats requested, as when
   // tasks are added they may have higher priority than the one
   // they're being merged with, or be requesting different formats. 
   // this ensures that high priority tasks still get processed
   // quickly, even if they're merged with existing low priority tasks.
   struct add_subscriber
   {
      add_subscriber(tile_protocol const& tile, std::string const& addr,int priority)
         : tile_(tile),
           addr_(addr),
           priority_(priority) {}
        
      void operator() (task & t)
      {
         if (priority_ > t.priority())
            t.set_priority(priority_);
         t.add_subscriber(tile_,addr_);
         // union all the requested formats for the same metatile
         t.format = static_cast<protoFmt>(t.format | tile_.format);
      }
      
      tile_protocol const& tile_;
      std::string const& addr_;
      int priority_;
   };

   // marks the task as being processed.
   struct processed_fun
   {
      void operator() (task & t)
      {            
         t.set_processed(true);
      }
   };
   
   // unmarks the task as being processed and resets the timestamp so
   // that it doesn't immediately get resubmitted again.
   struct unprocessed_fun
   {
      void operator() (task & t)
      {            
         std::time_t current_time(std::time(0));
         t.set_processed(false);
         t.set_timestamp(current_time);
      }
   };
    
public:
   /* sets the task identified by the tile parameter as being processed.
    * 
    * this means that the task will not appear as available via the 
    * front() function unless it is resubmitted via the 
    * resubmit_older_than() function, which means it won't get send out
    * to other workers.
    */
   void set_processed(tile_protocol const& tile)
   {
      typedef cont_type::index<rendermq::metatile>::type meta_index_type;
      meta_index_type & index = queue.get<rendermq::metatile>();
      meta_index_type::iterator itr = index.find(task(tile,0));
      if (itr!=index.end())
      {            
         processed_fun op;
         index.modify(itr,op);
      }
   }
   
   /* resets all tasks in the queue which have been marked as being
    * processed for at least a timeout number of seconds.
    *
    * this is used to detect jobs which are running longer than expected,
    * possibly due to worker failure, and make them available to be 
    * processed by other workers.
    *
    * jobs are resubmitted only when they have been in the queue for at
    * least timeout seconds, are marked as being processed and are not
    * bulk requests.
    */
   void resubmit_older_than (int timeout)
   {
      typedef cont_type::index<rendermq::timestamp>::type index_type;
      index_type & index = queue.get<rendermq::timestamp>();
      index_type::iterator itr = index.begin();
      index_type::iterator end = index.end();
      std::time_t now = std::time(0);
      for (; itr!=end; ++itr) 
      {
         if (itr->status != cmdRenderBulk && now - itr->timestamp() >= timeout &&
             itr->processed())
         {
            LOG_INFO(boost::format("Resubmitting task: %1%") % static_cast<tile_protocol>(*itr));
            unprocessed_fun op;
            index.modify(itr,op);
         }
      }
   }

   /* add a new task to the queue with the given priority, possibly
    * merging it with tasks for the same metatile which are already on
    * the queue. 
    *
    * the address given is stored with the task and can be used to
    * route the finished job back to the endpoint which originated
    * it.
    *
    * returns true when the task was newly-added, false if the task
    * was merged with another already in the queue.
    */
   bool push(tile_protocol const& tile, std::string const& address, int priority)
   {
      tile_protocol meta(tile);
      meta.x &= ~(METATILE-1);
      meta.y &= ~(METATILE-1);
      // because jobs can come in at any time, including while the job
      // is out being rendered by a worker, it's necessary to play it
      // safe and always tell the worker to render. otherwise the
      // worker might think that no data is required back (bulk) and
      // then this broker wouldn't have anything to send to the
      // handler.
      meta.status = cmdRender;
      
      std::pair<cont_type::iterator,bool> result = queue.insert(rendermq::task(meta,priority)); 
      if (result.first != queue.end())
      {
         add_subscriber sub(tile,address,priority);
         queue.modify(result.first,sub);   
      }
      return result.second;
   }
   
   /* remove the highest priority item from the queue.
    *
    * note that this doesn't pay any attention to whether the item is
    * being processed or not, and should be used with care. a better
    * approach may be to use erase() with the tile/task which you want
    * to remove.
    */
   void pop() 
   {
      typedef cont_type::index<rendermq::priority>::type priority_index_type;
      priority_index_type & index = queue.get<rendermq::priority>();
      priority_index_type::iterator itr = index.begin();
      priority_index_type::iterator end = index.end();
      if (itr!=end) index.erase(itr);
   }
   
   /* remove a specific task from the queue.
    *
    * note that this removes the whole task - not just a single
    * subscribed instance.
    *
    * returns true if the task existed, and was removed. false if the
    * task was not found in the queue.
    */
   bool erase(tile_protocol const& tile)
   {
      typedef cont_type::index<rendermq::metatile>::type meta_index_type;
      meta_index_type & index = queue.get<rendermq::metatile>();
      meta_index_type::iterator itr = index.find(task(tile,0));
      if (itr!=index.end())
      {
         index.erase(*itr);
         return true;
      }
      return false;
   }
   
   /* returns the task corresponding to a given tile, or an empty
    * optional if it could not be found.
    *
    * note that the tile parameter must be metatile-aligned, or it
    * won't match any tasks.
    */
   boost::optional<task const&> get(tile_protocol const& tile) const
   {
      typedef cont_type::index<rendermq::metatile>::type meta_index_type;
      meta_index_type const& index = queue.get<rendermq::metatile>();
      meta_index_type::iterator itr = index.find(task(tile,0));
      boost::optional<task const&> result;
      if (itr!=index.end()) return  boost::optional<task const&>(*itr);
      return result;
   }
   
   /* returns an iterator range through all the tasks in the queue.
    */
   std::pair<priority_index_iterator,priority_index_iterator> tasks() const
   {        
      priority_index_type const& index = queue.get<rendermq::priority>();
      priority_index_iterator itr = index.begin();
      priority_index_iterator end = index.end();
      return std::make_pair<priority_index_iterator,priority_index_iterator>(itr,end);
   }
   
   /* returns the highest priority unprocessed task, if there is
    * one. otherwise returns an empty optional.
    *
    * FIXME: method name is misleading, should be front_unprocessed()?
    */
   boost::optional<task const&> front() const
   {
      typedef cont_type::index<rendermq::priority>::type priority_index_type;
      priority_index_type const& index = queue.get<rendermq::priority>();
      priority_index_type::iterator itr = index.begin();
      priority_index_type::iterator end = index.end();
      boost::optional<task const&> result;
      for (;itr!=end;++itr) 
      {
         if (!itr->processed())
            return boost::optional<task const&>(*itr);
      }
      return result;
   }
   
   /* returns the number of tasks in the queue, total.
    *
    * see count_unprocessed() if you want the number of available,
    * unprocessed tasks in the queue.
    */
   size_t size() const
   {
      return queue.size();
   }
   
   /* returns the number of available tasks in the queue.
    */
   size_t count_unprocessed() const
   {
      size_t count=0;
      typedef cont_type::index<rendermq::priority>::type priority_index_type;
      priority_index_type const& index = queue.get<rendermq::priority>();
      priority_index_type::iterator itr = index.begin();
      priority_index_type::iterator end = index.end();
      for (; itr!=end;++itr)
      {
         if (!itr->processed()) ++count;
      }
      return count;
   }
   
   /* removes all tasks from the queue.
    */
   void clear() { queue.clear();}

private:

   // the queue itself.
   cont_type queue;
};

} // namespace rendermq

#endif // TASK_QUEUE_HPP
