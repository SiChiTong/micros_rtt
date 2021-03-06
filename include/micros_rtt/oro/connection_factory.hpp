/* 
 *  connection_factory.hpp - micros connection factory
 *  Copyright (C) 2015 Zaile Jiang
 *  
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef MICROSRTT_CONN_FACTORY_HPP
#define MICROSRTT_CONN_FACTORY_HPP

#include "ros/ros.h"
#include "micros_rtt/connection_base.h"
#include "micros_rtt/oro/channel_element.hpp"
#include "micros_rtt/oro/mq_channel_element.hpp"
#include "micros_rtt/oro/conn_input_endpoint.hpp"
#include "micros_rtt/oro/conn_output_endpoint.hpp"
#include "micros_rtt/oro/data_lockfree.hpp"

namespace micros_rtt
{

/** This class provides the basic tools to create channels that represent
 * connections between two connections.
 *
 * The ports and type transports use these functions to setup connections.
 * The interface may change as the needs of these 'users' change.
 */
class ConnFactory
{
public:
  ConnFactory() {}
  ~ConnFactory() {}

  template<typename M>
  static ChannelElementBase* buildDataStorage(const M& initial_value = M())
  {
    typename DataObjectLockFree<M>::shared_ptr data_object;
    data_object.reset( new DataObjectLockFree<M>(initial_value) );
    ChannelDataElement<M>* result = new ChannelDataElement<M>(data_object);
    return result;
  }

  template<typename M>
  static ChannelElementBase::shared_ptr buildChannelInput(ConnectionBasePtr publication, ChannelElementBase::shared_ptr output_channel)
  {
    ChannelElementBase::shared_ptr endpoint = new ConnInputEndpoint<M>(publication);
    if (output_channel)
      endpoint->setOutput(output_channel);
    return endpoint;
  }

  template<typename M>
  static ChannelElementBase::shared_ptr buildChannelOutput(ConnectionBasePtr subscription)
  {
    ChannelElementBase::shared_ptr endpoint = new ConnOutputEndpoint<M>(subscription);
    return endpoint;
  }
  
  template<typename M>
  static ChannelElementBase::shared_ptr buildBufferedChannelOutput(ConnectionBasePtr subscription, M const& initial_value = M() )
  {
    ChannelElementBase::shared_ptr endpoint = new ConnOutputEndpoint<M>(subscription);
    ChannelElementBase::shared_ptr data_object = buildDataStorage<M>(initial_value);
    data_object->setOutput(endpoint);
    return data_object;
  }

  template<typename M>
  static bool createConnection(ConnectionBasePtr publication, ConnectionBasePtr subscription)
  {
    // This is the input channel element of the output half
    ChannelElementBase::shared_ptr output_half = 0;
    // local ports, create buffer here.
    output_half = buildBufferedChannelOutput<M>(subscription);

    if (!output_half)
    {
      ROS_WARN("micros connection factory fail to build buffered channel output of topic:%s.", publication->getTopic().c_str());
      return false;
    }

    // Since output is local, buildChannelInput is local as well.
    // This this the input channel element of the whole connection
    ChannelElementBase::shared_ptr channel_input =
      buildChannelInput<M>(publication, output_half);
    ROS_DEBUG("micros connection factory has build all channel elements topic:%s needed, ready to check the connection.", publication->getTopic().c_str());

    return createAndCheckConnection(publication, subscription, channel_input);
  }

  template<class M>
  static bool createStream(ConnectionBasePtr connection, bool is_sender)
  {
    ChannelElementBase::shared_ptr chan;
    if (is_sender)
    {
      ROS_DEBUG("micros connection factory creating publication stream.");
      chan = buildChannelInput<M>(connection, ChannelElementBase::shared_ptr() );
        
      ChannelElementBase::shared_ptr chan_stream = createMqStream<M>(connection, true);
      if ( !chan_stream ) 
      {
        ROS_WARN("micros connection factory failed to create channel stream  for publication:%s", connection->getTopic().c_str());
        return false;
      }
      
      chan->setOutput( chan_stream );
      return createAndCheckStream(connection, chan, true);
    }
    else
    {
      ROS_DEBUG("micros connection factory creating subscription stream.");
      chan = buildChannelOutput<M>(connection);
      
      // note: don't refcount this final input chan, because no one will
      // take a reference to it. It would be destroyed upon return of this function.
      ChannelElementBase::shared_ptr chan_stream = createMqStream<M>(connection, false);
      
      if ( !chan_stream ) 
      {
        ROS_WARN("micros connection factory failed to create channel stream for subscription:%s", connection->getTopic().c_str());
        return false;
      }
    
      chan_stream->getOutputEndPoint()->setOutput( chan );
      return createAndCheckStream(connection, chan_stream, false);
    }
    
  }

protected:
  static bool createAndCheckConnection(ConnectionBasePtr publication, 
          ConnectionBasePtr subscription, ChannelElementBase::shared_ptr channel_input);

  static bool createAndCheckStream(ConnectionBasePtr connection, 
          ChannelElementBase::shared_ptr chan, bool is_sender);
  
  template<typename M>
  static ChannelElementBase::shared_ptr createMqStream(ConnectionBasePtr connection, bool is_sender) 
  {
    try
    {
      ChannelElementBase::shared_ptr mq = new MQChannelElement<M>(connection, is_sender);
      if (!is_sender)
      {
        //now we get message directly from message queue.
        //in the future, the receiver will have a buffer to store his messages in.
        //ChannelElementBase::shared_ptr buf = buildDataStorage<M>();
        //mq->setOutput(buf);
      }
      return mq;
    }
    catch(std::exception& e)
    {
      ROS_INFO("micros connection factory failed to create MQueue Channel element for topic:%s", connection->getTopic().c_str());
    }
    return ChannelElementBase::shared_ptr();
  } 

};

}
#endif

