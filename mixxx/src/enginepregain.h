/***************************************************************************
                          enginepregain.h  -  description
                             -------------------
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ENGINEPREGAIN_H
#define ENGINEPREGAIN_H

#include <qobject.h>

#include "engineobject.h"
#include "midiobject.h"
#include "controllogpotmeter.h"

class EnginePregain : public EngineObject {
  Q_OBJECT
public:
  EnginePregain(const char *group);
  ~EnginePregain();
  CSAMPLE *process(const CSAMPLE*, const int);

  ControlLogpotmeter* potmeter;
public slots:
  void slotUpdate(FLOAT_TYPE);

private:
  CSAMPLE *buffer;
  FLOAT_TYPE pregain;
};

#endif
