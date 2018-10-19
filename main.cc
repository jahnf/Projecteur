#include "projecteurapp.h"

#ifndef NDEBUG
#include <QQmlDebuggingEnabler>
#endif

int main(int argc, char *argv[])
{
  //QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  ProjecteurApplication app(argc, argv);
  return app.exec();
}
