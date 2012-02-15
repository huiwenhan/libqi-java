/*
** Author(s):
**  - Laurent LEC   <llec@aldebaran-robotics.com>
**
** Copyright (C) 2012 Aldebaran Robotics
*/

#ifndef   	QT_QITRANSPORTSOCKET_H_
# define   	QT_QITRANSPORTSOCKET_H_

#include <QtCore/QObject>
#include <qimessaging/transport/transport_socket.hpp>

class QiTransportSocketPrivate;

class QiTransportSocket : public QObject {
public:
  QiTransportSocket();
  QiTransportSocket(int fd, struct event_base *base);
  virtual ~QiTransportSocket();

  bool connect(const std::string &address,
               unsigned short port,
               struct event_base *base);
  void disconnect();

  friend class QiTransportServer;

protected:
  QiTransportSocketPrivate *_p;
};


class QiTransportSocketPrivate
{
public:
  qi::TransportSocket *socket;
};

#endif 	    /* !QITRANSPORTSOCKET_H_ */