/*! @file
  @brief
  mruby/c irep file writer.

  <pre>
  Copyright (C) 2017-      Kyushu Institute of Technology.
  Copyright (C) 2017-2026 Shimane IT Open-Innovation Center.
  Copyright (C) 2026-      Shimane Institute for Industrial Technology.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

#include <QCoreApplication>

#include "mrbwrite.h"


int main(int argc, char *argv[])
{
  MrbWrite app(argc, argv);
  return app.exec();
}
