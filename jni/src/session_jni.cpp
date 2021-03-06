/*
**
** Author(s):
**  - Pierre ROULLON <proullon@aldebaran-robotics.com>
**
** Copyright (C) 2012, 2013 Aldebaran Robotics
** See COPYING for the license
*/

#include <stdexcept>

#include <qi/log.hpp>
#include <qi/api.hpp>
#include <qi/anyfunction.hpp>
#include <qi/session.hpp>

#include <jni.h>
#include <jnitools.hpp>
#include <session_jni.hpp>
#include <object_jni.hpp>
#include <callbridge.hpp>

#include <qi/messaging/clientauthenticator.hpp>
#include <qi/messaging/clientauthenticatorfactory.hpp>

qiLogCategory("qimessaging.jni");

JNIEXPORT jlong JNICALL Java_com_aldebaran_qi_Session_qiSessionCreate(JNIEnv *QI_UNUSED(env), jobject QI_UNUSED(obj))
{
  qi::Session *session = new qi::Session();

  return (jlong) session;
}

JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_qiSessionDestroy(JNIEnv *QI_UNUSED(env), jobject QI_UNUSED(obj), jlong pSession)
{
  qi::Session *s = reinterpret_cast<qi::Session*>(pSession);
  delete s;
}

JNIEXPORT jboolean JNICALL Java_com_aldebaran_qi_Session_qiSessionIsConnected(JNIEnv *QI_UNUSED(env), jobject QI_UNUSED(obj), jlong pSession)
{
  qi::Session *s = reinterpret_cast<qi::Session*>(pSession);

  return (jboolean) s->isConnected();
}

static void adaptFuture(qi::Future<void> f, qi::Promise<qi::AnyValue> p)
{
  if (f.hasError())
    p.setError(f.error());
  else
    p.setValue(qi::AnyValue(qi::typeOf<void>()));
}

JNIEXPORT jlong JNICALL Java_com_aldebaran_qi_Session_qiSessionConnect(JNIEnv *env, jobject QI_UNUSED(obj), jlong pSession, jstring jurl)
{
  if (pSession == 0)
  {
    throwNewException(env, "Given qi::Session doesn't exists (pointer null).");
    return 0;
  }

  // After this function return, callbacks are going to be set on new created Future.
  // Save the JVM pointer here to avoid big issues when callbacks will be called.
  JVM(env);
  qi::Future<qi::AnyValue> *fref = new qi::Future<qi::AnyValue>();
  qi::Promise<qi::AnyValue> promise;
  *fref = promise.future();
  qi::Session *s = reinterpret_cast<qi::Session*>(pSession);
  try
  {
    qi::Future<void> f = s->connect(qi::jni::toString(jurl));
    f.connect(adaptFuture, _1, promise);
  }
  catch (const std::exception& e)
  {
    promise.setError(e.what());
  }
  return (jlong) fref;
}

JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_qiSessionClose(JNIEnv *QI_UNUSED(env), jobject QI_UNUSED(obj), jlong pSession)
{
  qi::Session *s = reinterpret_cast<qi::Session*>(pSession);

  s->close();
}

JNIEXPORT jlong JNICALL Java_com_aldebaran_qi_Session_service(JNIEnv *env, jobject QI_UNUSED(obj), jlong pSession, jstring jname)
{
  qi::Session *s = reinterpret_cast<qi::Session*>(pSession);
  std::string serviceName = qi::jni::toString(jname);

  try
  {
    qi::Future<qi::AnyObject> serviceFuture = s->service(serviceName);
    // qiFutureCallGet() requires that the future returns a qi::AnyValue
    qi::Future<qi::AnyValue> future = qi::toAnyValueFuture(std::move(serviceFuture));
    auto futurePtr = new qi::Future<qi::AnyValue>(std::move(future));
    return reinterpret_cast<jlong>(futurePtr);
  }
  catch (std::runtime_error &e)
  {
    throwNewRuntimeException(env, e.what());
    return 0;
  }
}

JNIEXPORT jint JNICALL Java_com_aldebaran_qi_Session_registerService(JNIEnv *env, jobject QI_UNUSED(obj), jlong pSession, jstring jname, jobject object)
{
  qi::Session*    session = reinterpret_cast<qi::Session*>(pSession);
  std::string     name    = qi::jni::toString(jname);
  JNIObject obj(object);
  jint ret = 0;

  try
  {
    ret = session->registerService(name, obj.objectPtr());
  }
  catch (std::runtime_error &e)
  {
    qiLogError() << "Throwing exception : " << e.what();
    throwNewException(env, e.what());
    return 0;
  }

  if (ret <= 0)
  {
    throwNewException(env, "Cannot register service");
    return 0;
  }

  return ret;
}

JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_unregisterService(JNIEnv *QI_UNUSED(env), jobject QI_UNUSED(obj), jlong pSession, jint serviceId)
{
  qi::Session*    session = reinterpret_cast<qi::Session*>(pSession);
  unsigned int    id = static_cast<unsigned int>(serviceId);

  session->unregisterService(id);
}

JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_onDisconnected(JNIEnv *env, jobject jobj, jlong pSession, jstring jcallbackName, jobject jobjectInstance)
{
  extern MethodInfoHandler gInfoHandler;
  qi::Session*    session = reinterpret_cast<qi::Session*>(pSession);
  std::string     callbackName = qi::jni::toString(jcallbackName);
  std::string     signature;
  qi_method_info*            data;

  // Create a new global reference on object instance.
  // jobject structure are local reference and are destroyed when returning to JVM
  jobjectInstance = env->NewGlobalRef(jobjectInstance);

  // Create a struct holding a jobject instance, jmethodId id and other needed thing for callback
  // Pass it to void * data to register_method
  signature = callbackName + "::(s)";
  // FIXME jobj is not a global ref, it may be invalid when it will be used
  data = new qi_method_info(jobjectInstance, signature, jobj);
  gInfoHandler.push(data);

  session->disconnected.connect(
      qi::AnyFunction::fromDynamicFunction(
          boost::bind(&event_callback_to_java, (void*) data, _1)));
}

JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_addConnectionListener(JNIEnv *env, jobject QI_UNUSED(obj), jlong pSession, jobject listener)
{
  qi::Session *session = reinterpret_cast<qi::Session*>(pSession);
  auto gListener = qi::jni::makeSharedGlobalRef(env, listener);
  session->connected.connect([gListener] {
    qi::jni::JNIAttach attach;
    JNIEnv *env = attach.get();
    qi::jni::Call<void>::invoke(env, gListener.get(), "onConnected", "()V");
  });
  session->disconnected.connect([gListener](const std::string &reason) {
    qi::jni::JNIAttach attach;
    JNIEnv *env = attach.get();
    qi::jni::Call<void>::invoke(env, gListener.get(), "onDisconnected", "(Ljava/lang/String;)V", qi::jni::toJstring(reason));
  });
}

class Java_ClientAuthenticator : public qi::ClientAuthenticator
{
public:
  Java_ClientAuthenticator(JNIEnv* env, jobject object)
  {
    env->GetJavaVM(&_jvm);
    _jobjectRef = qi::jni::makeSharedGlobalRef(env, object);
  }

  qi::CapabilityMap initialAuthData()
  {
    JNIEnv* env = nullptr;
#ifndef ANDROID
    _jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
#else
    _jvm->AttachCurrentThread(&env, nullptr);
#endif
    jobject _jobject = _jobjectRef.get();
    jobject ca = qi::jni::Call<jobject>::invoke(env, _jobject, "initialAuthData", "()Ljava/util/Map;");
    auto result = JNI_JavaMaptoMap(env, ca);
    env->DeleteLocalRef(ca);
    return result;
  }

  qi::CapabilityMap _processAuth(const qi::CapabilityMap &authData)
  {
    JNIEnv* env = nullptr;
#ifndef ANDROID
    _jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
#else
    _jvm->AttachCurrentThread(&env, nullptr);
#endif
    jobject _jobject = _jobjectRef.get();
    jobject jmap = JNI_MapToJavaMap(env, authData);
    jobject ca = qi::jni::Call<jobject>::invoke(env, _jobject, "_processAuth", "(Ljava/util/Map;)Ljava/util/Map;", jmap);
    env->DeleteLocalRef(jmap);
    auto result = JNI_JavaMaptoMap(env, ca);
    env->DeleteLocalRef(ca);
    return result;
  }

private:
  JavaVM* _jvm;
  qi::jni::SharedGlobalRef _jobjectRef;

  static qi::CapabilityMap JNI_JavaMaptoMap(JNIEnv* env, jobject jmap)
  {
    qi::CapabilityMap result;

    jobject set = qi::jni::Call<jobject>::invoke(env, jmap, "entrySet", "()Ljava/util/Set;");
    jobject it = qi::jni::Call<jobject>::invoke(env, set, "iterator", "()Ljava/util/Iterator;");
    env->DeleteLocalRef(set);

    while (qi::jni::Call<jboolean>::invoke(env, it, "hasNext", "()Z"))
    {
      jobject entry = qi::jni::Call<jobject>::invoke(env, it, "next", "()Ljava/lang/Object;");
      jstring key = (jstring) qi::jni::Call<jobject>::invoke(env, entry, "getKey", "()Ljava/lang/Object;");
      std::string k = env->GetStringUTFChars(key, nullptr);

      // FIXME: should implement this for jobject and not only jstring
      qiLogWarning() << "FIXME: only jstring are supported";
      jobject value = qi::jni::Call<jobject>::invoke(env, entry, "getValue", "()Ljava/lang/Object;");
      std::string v2 = env->GetStringUTFChars((jstring) value, nullptr);

      env->DeleteLocalRef(entry);
      env->DeleteLocalRef(key);
      env->DeleteLocalRef(value);

      qi::AnyValue v = qi::AnyValue::from(v2);

      result[k] = v;
    }

    env->DeleteLocalRef(it);

    return result;
  }

  static jobject JNI_MapToJavaMap(JNIEnv* env, qi::CapabilityMap map)
  {
    jclass mapClass = env->FindClass("java/util/HashMap");
    jmethodID init = env->GetMethodID(mapClass, "<init>", "()V");
    jobject result = env->NewObject(mapClass, init);
    env->DeleteLocalRef(mapClass);

    qi::CapabilityMap::const_iterator it;
    for (it = map.begin(); it != map.end(); ++it)
    {
      std::string key = it->first;
      jstring k = env->NewStringUTF(key.c_str());

      const qi::AnyValue& val = it->second;
      const std::string sig = val.signature().toString();
      jobject v;

      if (sig == "I")
      {
        jclass cls = env->FindClass("java/lang/Integer");
        jmethodID mid = env->GetMethodID(cls, "<init>", "(I)V");
        v = env->NewObject(cls, mid, val.toUInt());
        env->DeleteLocalRef(cls);
      }
      else if (sig == "s")
      {
        v = (jobject) env->NewStringUTF(val.toString().c_str());
      }
      else
      {
        env->DeleteLocalRef(k);
        qiLogError() << "sig " << sig << " not supported, skipping...";
        continue;
      }

      qi::jni::Call<jobject>::invoke(env, result, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", k, v);

      env->DeleteLocalRef(k);
      env->DeleteLocalRef(v);
    }

    return result;
  }
};

class Java_ClientAuthenticatorFactory : public qi::ClientAuthenticatorFactory
{
public:
  Java_ClientAuthenticatorFactory(JNIEnv* env, jobject object)
  {
    env->GetJavaVM(&_jvm);
    _jobject = env->NewGlobalRef(object);
  }

  qi::ClientAuthenticatorPtr newAuthenticator()
  {
    JNIEnv* env = nullptr;
#ifndef ANDROID
    _jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
#else
    _jvm->AttachCurrentThread(&env, nullptr);
#endif
    jobject ca = qi::jni::Call<jobject>::invoke(env, _jobject, "newAuthenticator", "()Lcom/aldebaran/qi/ClientAuthenticator;");
    auto result = boost::make_shared<Java_ClientAuthenticator>(env, ca);
    env->DeleteLocalRef(ca);
    return result;
  }

private:
  JavaVM* _jvm;
  jobject _jobject;
};

JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_setClientAuthenticatorFactory(JNIEnv* env, jobject QI_UNUSED(obj), jlong pSession, jobject object)
{
  qi::Session* session = reinterpret_cast<qi::Session*>(pSession);
  session->setClientAuthenticatorFactory(boost::make_shared<Java_ClientAuthenticatorFactory>(env, object));
}

JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_loadService(JNIEnv *env, jobject obj, jlong pSession, jstring jname)
{
  qi::Session* session = reinterpret_cast<qi::Session*>(pSession);
  std::string moduleName = qi::jni::toString(jname);
  session->loadService(moduleName);
  return;
}

/**
 * @brief List of URL session end points.
 * @param env JNI environment.
 * @param obj Object Java instance.
 * @param pSession Reference to session in JNI.
 * @param endpointsList List to add URL session endpoints. (List<String>)
 */
JNIEXPORT void JNICALL Java_com_aldebaran_qi_Session_endpoints(JNIEnv * env, jobject obj, jlong pSession, jobject endpointsList)
{
  qi::Session* session = reinterpret_cast<qi::Session*>(pSession);
  std::vector<qi::Url> endpoints = session->endpoints();
  jmethodID methodAdd = env->GetMethodID(cls_list, "add", "(Ljava/lang/Object;)Z");

  for (std::vector<qi::Url>::iterator it = endpoints.begin(); it != endpoints.end(); ++it)
  {
    jstring url = qi::jni::toJstring((*it).str());
    env->CallBooleanMethod(endpointsList, methodAdd, url);
    qi::jni::releaseString(url);
  }
}
