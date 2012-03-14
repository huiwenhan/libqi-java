/*
** Author(s):
**  - hcuche <hcuche@aldebaran-robotics.com>
**
** Copyright (C) 2012 hcuche
*/

#pragma once
#ifndef _QIMESSAGING_BUFFER_HPP_
#define _QIMESSAGING_BUFFER_HPP_

# include <boost/shared_ptr.hpp>
# include <qimessaging/api.hpp>
# include <cstdlib>

namespace qi
{
  class BufferPrivate;

  class QIMESSAGING_API Buffer
  {
  public:
    Buffer();

    size_t write(const void *data, size_t size);
    size_t read(void *data, size_t size);
    // equivalent to peek() && seek()
    void  *read(size_t size);
    size_t size() const;
    void  *reserve(size_t size);
    size_t seek(long offset);
    void  *peek(size_t size) const;

    void  *data() const;
    void   dump() const;

  private:
    boost::shared_ptr<BufferPrivate> _p;
  };


} // !qi

#endif  // _QIMESSAGING_BUFFER_HPP_
