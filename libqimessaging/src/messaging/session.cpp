/*
*  Author(s):
*  - Cedric Gestes <gestes@aldebaran-robotics.com>
*  - Herve Cuche <hcuche@aldebaran-robotics.com>
*
*  Copyright (C) 2012 Aldebaran Robotics
*/

#include <qimessaging/session.hpp>
#include <qimessaging/datastream.hpp>
#include <qimessaging/transport.hpp>

static int uniqueRequestId = 0;

namespace qi {

Session::Session()
{
  _nthd = new qi::NetworkThread();

  tc = new qi::TransportSocket();
  tc->setDelegate(this);
}

Session::~Session()
{
  tc->disconnect();
  delete tc;
}

void Session::connect(const std::string &masterAddress)
{
  size_t begin = 0;
  size_t end = 0;
  end = masterAddress.find(":");

  std::string ip = masterAddress.substr(begin, end);
  begin = end + 1;

  unsigned int port;
  std::stringstream ss(masterAddress.substr(begin));
  ss >> port;

  tc->connect(ip, port, _nthd->getEventBase());
}

bool Session::disconnect()
{
  return true;
}


bool Session::waitForConnected(int msecs)
{
  return tc->waitForConnected(msecs);
}

bool Session::waitForDisconnected(int msecs)
{
  return tc->waitForDisconnected(msecs);
}

void Session::registerEndpoint(const qi::EndpointInfo &e)
{
  qi::DataStream d;
  d << e;

  qi::Message msg;
  msg.setId(uniqueRequestId++);
  msg.setSource(_name);
  msg.setDestination(_destination);
  msg.setPath("registerEndpoint");
  msg.setData(d.str());

  tc->send(msg);
  tc->waitForId(msg.id());
  qi::Message ans;
  tc->read(msg.id(), &ans);
}

void Session::unregisterEndpoint(const qi::EndpointInfo& e)
{
  qi::DataStream d;
  d << e;

  qi::Message msg;
  msg.setId(uniqueRequestId++);
  msg.setSource(_name);
  msg.setDestination(_destination);
  msg.setPath("unregisterEndpoint");
  msg.setData(d.str());

  tc->send(msg);
  tc->waitForId(msg.id());
  qi::Message ans;
  tc->read(msg.id(), &ans);
}

bool Session::isInitialized() const
{
  return true;
}

std::vector<std::string> Session::machines()
{
  std::vector<std::string> result;

  qi::Message msg;
  msg.setId(uniqueRequestId++);
  msg.setSource(_name);
  msg.setDestination(_destination);
  msg.setPath("machines");

  tc->send(msg);
  tc->waitForId(msg.id());
  qi::Message ans;
  tc->read(msg.id(), &ans);

  return result;
}

std::vector<std::string> Session::services()
{
  std::vector<std::string> result;

  qi::Message msg;
  msg.setId(uniqueRequestId++);
  msg.setSource(_name);
  msg.setDestination(_destination);
  msg.setPath("services");

  tc->send(msg);

  tc->waitForId(msg.id());
  qi::Message ans;
  tc->read(msg.id(), &ans);

  qi::DataStream d(ans.data());
  d >> result;

  return result;
}

qi::TransportSocket* Session::service(const std::string &name,
                                     const std::string &type)
{
  std::vector<qi::EndpointInfo> result;

  qi::Message msg;

  msg.setId(uniqueRequestId++);
  msg.setSource(_name);
  msg.setDestination(_destination);
  msg.setPath("service");
  msg.setData(name);

  tc->send(msg);

  tc->waitForId(msg.id());
  qi::Message ans;
  tc->read(msg.id(), &ans);

  qi::DataStream d(ans.data());
  d >> result;

  qi::TransportSocket* ts = NULL;
  std::vector<qi::EndpointInfo>::iterator endpointIt;
  for (endpointIt = result.begin(); endpointIt != result.end(); ++endpointIt)
  {
    if (endpointIt->type == type)
    {
      ts = new qi::TransportSocket();
      ts->setDelegate(this);
      ts->connect(endpointIt->ip, endpointIt->port, _nthd->getEventBase());
      ts->waitForConnected();
    }
  }

  return ts;
}


void Session::onConnected(const qi::Message &msg)
{
  //  std::cout << "connected broker: " << std::endl;
}

void Session::onWrite(const qi::Message &msg)
{
  //  std::cout << "written broker: " << std::endl;
}

void Session::onRead(const qi::Message &msg)
{
  //  std::cout << "read broker: " << std::endl;
}

}