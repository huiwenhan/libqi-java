/*
** Author(s):
**  - Cedric GESTES <gestes@aldebaran-robotics.com>
**
** Copyright (C) 2012 Cedric GESTES
*/

#ifndef _QIMESSAGING_DETAILS_FUTURE_HPP_
#define _QIMESSAGING_DETAILS_FUTURE_HPP_

namespace qi {

  namespace detail {

    class FutureBasePrivate;
    class QIMESSAGING_API FutureBase {
    public:
      FutureBase();
      ~FutureBase();
      bool waitForValue(int msecs = 30000) const;
      void setReady();
      bool isReady() const;

    public:
      FutureBasePrivate *_p;
    };

    //common state shared between a Promise and multiple Futures
    template <typename T>
    class FutureState : public FutureBase {
    public:
      FutureState(Future<T> *fut)
        : _self(fut),
          _value(),
          _callback(0),
          _data(0)
      {
      }

      void setValue(const T &value)
      {
        _value = value;
        setReady();
        if (_callback) {
            _callback->onFutureFinished(*_self, _data);
          }
      }

      void setCallback(FutureInterface<T> *interface, void *data) {
        _callback = interface;
        _data     = data;
      }

      const T &value() const    { waitForValue(); return _value; }
      T &value()                { waitForValue(); return _value; }

    private:
      Future<T>          *_self;
      T                   _value;
      FutureInterface<T> *_callback;
      void               *_data;
    };


  } // namespace detail

  namespace detail {
    //void specialisation: do not hold anything only used for synchronisation
    template <>
    class FutureState<void> : public FutureBase {
    public:
      FutureState(Future<void> *fut)
        : _self(fut),
          _callback(0),
          _data(0)
      {
      }

      void setValue(const void *QI_UNUSED(value))
      {
        setReady();
        if (_callback) {
            _callback->onFutureFinished(*_self, _data);
          }
      }

      void setCallback(FutureInterface<void> *interface, void *data) {
        _callback = interface;
        _data     = data;
      }

      void value() const    { waitForValue(); }

    private:
      Future<void>          *_self;
      FutureInterface<void> *_callback;
      void                  *_data;
    };
  }

  //void specialisation: do not hold anything only used for synchronisation
  template <>
  class Future<void> {
  public:
    Future()
      : _p(boost::shared_ptr< detail::FutureState<void> >())
    {
      _p = boost::shared_ptr< detail::FutureState<void> >(new detail::FutureState<void>(this));
    }

    void value() const    { _p->waitForValue(); }

    bool waitForValue(int msecs = 30000) const { return _p->waitForValue(msecs); }
    bool isReady() const                       { return _p->isReady(); }

    void setCallback(FutureInterface<void> *interface, void *data = 0) {
      _p->setCallback(interface, data);
    }

    friend class Promise<void>;
  private:
    boost::shared_ptr< detail::FutureState<void> > _p;
  };

  template <>
  class Promise<void> {
  public:
    Promise() { }

    void setValue(const void *value) {
      _f._p->setValue(value);
    }

    Future<void> future() { return _f; }

  protected:
    Future<void> _f;
  };

};

#endif