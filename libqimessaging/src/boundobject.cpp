/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#include <qitype/objecttypebuilder.hpp>
#include "boundobject.hpp"
#include "serverresult.hpp"

static const int gObjectOffset = 10;

namespace qi {

  static GenericValue forwardEvent(const GenericFunctionParameters& params,
                                   unsigned int service, unsigned int event, TransportSocketPtr client)
  {
    qi::Message msg;
    msg.setParameters(params);
    msg.setService(service);
    msg.setFunction(event + gObjectOffset);
    msg.setType(Message::Type_Event);
    msg.setObject(Message::GenericObject_Main);
    client->send(msg);
    return GenericValue();
  }


  ServiceBoundObject::ServiceBoundObject(unsigned int serviceId, qi::ObjectPtr object, qi::MetaCallType mct)
    : _links()
    , _serviceId(serviceId)
    , _object(object)
    , _callType(mct)
  {
    _self = createServiceBoundObjectType(this);
  }

  ServiceBoundObject::~ServiceBoundObject()
  {
  }

  qi::ObjectPtr ServiceBoundObject::createServiceBoundObjectType(ServiceBoundObject *self) {
    qi::ObjectTypeBuilder<ServiceBoundObject> ob;

    ob.advertiseMethod("registerEvent"  , &ServiceBoundObject::registerEvent);
    ob.advertiseMethod("unregisterEvent", &ServiceBoundObject::unregisterEvent);
    ob.advertiseMethod("metaObject"     , &ServiceBoundObject::metaObject);
    return ob.object(self);
  }

  //Bound Method
  unsigned int ServiceBoundObject::registerEvent(unsigned int objectId, unsigned int event, unsigned int remoteLinkId) {
    unsigned int eventId = event - gObjectOffset;
    //throw on error
    GenericFunction mc = makeDynamicGenericFunction(boost::bind(&forwardEvent, _1, _serviceId, eventId, currentSocket()));
    unsigned int linkId = _object->connect(eventId, mc);

    _links[currentSocket()][remoteLinkId] = RemoteLink(linkId, eventId);
    return linkId;
  }

  //Bound Method
  void ServiceBoundObject::unregisterEvent(unsigned int objectId, unsigned int QI_UNUSED(event), unsigned int remoteLinkId) {
    //unsigned int eventId = event - gObjectOffset;
    //throw on error
    ServiceLinks&          sl = _links[currentSocket()];
    ServiceLinks::iterator it = sl.find(remoteLinkId);

    if (it == sl.end())
    {
      std::stringstream ss;
      ss << "Unregister request failed for " << remoteLinkId <<" " << objectId;
      qiLogError("qi::Server") << ss.str();
      throw std::runtime_error(ss.str());
    }
    _object->disconnect(it->second.localLinkId);
  }


  //Bound Method
  qi::MetaObject ServiceBoundObject::metaObject(unsigned int objectId) {
    //we inject specials methods here
    return qi::MetaObject::merge(_self->metaObject(), gObjectOffset, _object->metaObject());
  }

  void ServiceBoundObject::onMessage(const qi::Message &msg, TransportSocketPtr socket) {
    qi::ObjectPtr    obj;
    unsigned int     funcId;
    qi::MetaCallType mct;

    _currentSocket = socket;
    //choose between special function (on BoundObject) or normal calls
    if (msg.function() < gObjectOffset) {
      obj = _self;
      mct = MetaCallType_Direct;
      funcId = msg.function();
    } else {
      obj = _object;
      mct = _callType;
      funcId = msg.function() - gObjectOffset;
    }

    std::string sigparam;
    GenericFunctionParameters mfp;

    if (msg.type() == qi::Message::Type_Call) {
      const qi::MetaMethod *mm = obj->metaObject().method(funcId);
      if (!mm) {
        std::stringstream ss;
        ss << "No such method " << msg.address();
        qiLogError("qi::Server") << ss.str();
        qi::Promise<GenericValue> prom;
        prom.setError(ss.str());
        serverResultAdapter(prom.future(), socket, msg.address());
        return;
      }
      sigparam = mm->signature();
    }

    if (msg.type() == qi::Message::Type_Event) {
      const qi::MetaSignal *ms = obj->metaObject().signal(funcId);
      if (ms)
        sigparam = ms->signature();
      else {
        const qi::MetaMethod *mm = obj->metaObject().method(funcId);
        if (mm)
          sigparam = mm->signature();
        else {
          qiLogError("qi::Server") << "No such signal/method on event message " << msg.address();
          return;
        }
      }
    }


    sigparam = signatureSplit(sigparam)[2];
    sigparam = sigparam.substr(1, sigparam.length()-2);
    //socket object always take the TransportSocketPtr as last parameter, inject it!
    mfp = msg.parameters(sigparam);

    switch (msg.type())
    {
    case Message::Type_Call: {
         qi::Future<GenericValue> fut = obj->metaCall(funcId, mfp, _callType);
         fut.connect(boost::bind<void>(&serverResultAdapter, _1, socket, msg.address()));
      }
      break;
    case Message::Type_Event: {
        obj->metaEmit(funcId, mfp);
      }
      break;
    default:
        qiLogError("qi.Server") << "unknown request of type " << (int)msg.type() << " on service: " << msg.address();
    }
    //########################
    mfp.destroy();
    _currentSocket.reset();
  }

  void ServiceBoundObject::onSocketDisconnected(TransportSocketPtr client, int error)
  {
    // Disconnect event links set for this client.
    BySocketServiceLinks::iterator it = _links.find(client);
    if (it != _links.end())
    {
      for (ServiceLinks::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
        _object->disconnect(jt->second.localLinkId);
      _links.erase(it);
    }
  }

  qi::BoundObjectPtr makeServiceBoundObjectPtr(unsigned int serviceId, qi::ObjectPtr object) {
    boost::shared_ptr<ServiceBoundObject> ret(new ServiceBoundObject(serviceId, object));
    return ret;
  }


}