/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#ifndef _QIMESSAGING_OBJECTHOST_HPP_
#define _QIMESSAGING_OBJECTHOST_HPP_

#include <map>

#include <boost/thread/mutex.hpp>

#include <qi/atomic.hpp>

#include <qitype/fwd.hpp>

#include <qimessaging/transportsocket.hpp>

namespace qi
{
  class Message;
  class ServiceBoundObject;
  class ObjectHost
  {
  public:
    ObjectHost(unsigned int service);
    ~ObjectHost();
    void onMessage(const qi::Message &msg, TransportSocketPtr socket);
    unsigned int addObject(ServiceBoundObject* obj, unsigned int objId = 0);
    void removeObject(unsigned int);
    unsigned int service() { return _service;}
    unsigned int nextId() { return ++_nextId;}
    qi::Signal<void()> onDestroy;
  private:
    typedef std::map<unsigned int, ServiceBoundObject*> ObjectMap;
    boost::mutex  _mutex;
    unsigned int _service;
    ObjectMap _objectMap;
    qi::Atomic<long> _nextId;
  };
}

#endif