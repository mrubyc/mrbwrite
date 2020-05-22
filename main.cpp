/*! @file
  @brief
  mruby/c irep file writer.

  <pre>
  Copyright (C) 2017 Kyushu Institute of Technology.
  Copyright (C) 2017 Shimane IT Open-Innovation Center.

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
