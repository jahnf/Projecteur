#include "projecteurapp.h"
#include "projecteur-GitVersion.h"

#include <QCommandLineParser>

#ifndef NDEBUG
#include <QQmlDebuggingEnabler>
#endif

#include <iostream>

namespace {
  struct print {
    template<typename T>
    auto& operator<<(const T& a) const { return std::cout << a; }
    ~print() { std::cout << std::endl; }
  };
}

int main(int argc, char *argv[])
{
  {
    QCommandLineParser parser;
    parser.setApplicationDescription("Linux/X11 application for the Logitech Spotlight device.");
    QCommandLineOption versionOption(QStringList{ "v", "version"}, "Print version information.");
    parser.addOption(versionOption);

    QStringList args;
    for(int i = 0; i < argc; ++i) {
      args.push_back(argv[i]);
    }
    parser.process(args);
    if (parser.isSet(versionOption))
    {
      print() << "Projecteur " << projecteur::version_string();
      if (std::string( projecteur::version_branch()) != "master")
      { // Not a build from master branch, print out additional information:
        print() << "  - git-branch: " << projecteur::version_branch();
        print() << "  - git-hash: " << projecteur::version_fullhash();
      }
      // Show if we have a build from modified sources
      if (projecteur::version_isdirty())
        print() << "  - dirty-flag: " << projecteur::version_isdirty();
      return 0;
    }
  }

  //QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  ProjecteurApplication app(argc, argv);
  app.setApplicationName("Projecteur");
  app.setApplicationVersion(projecteur::version_string());
  return app.exec();
}
