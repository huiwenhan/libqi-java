/*
**
** Author(s):
**  - Pierre ROULLON <proullon@aldebaran-robotics.com>
**
** Copyright (C) 2012 Aldebaran Robotics
*/

#pragma once
#ifndef JOBJECTCONVERTER_HPP_
#define JOBJECTCONVERTER_HPP_

#include <jni.h>
#include <qitype/type.hpp>

jobject JObject_from_GenericValue(qi::GenericValuePtr val);
void JObject_from_GenericValue(qi::GenericValuePtr val, jobject* target);
std::pair<qi::GenericValuePtr, bool> GenericValue_from_JObject(jobject val);

#endif // !JOBJECTCONVERTER_HPP_