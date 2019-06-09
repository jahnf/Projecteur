// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "projecteurapp.h"
#include "projecteur-GitVersion.h"

#include "runguard.h"

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

  struct error {
    template<typename T>
    auto& operator<<(const T& a) const { return std::cerr << a; }
    ~error() { std::cerr << std::endl; }
  };
}

int main(int argc, char *argv[])
{
  QCoreApplication::setApplicationName("Projecteur");
  QCoreApplication::setApplicationVersion(projecteur::version_string());
  QString ipcCommand;
  {
    QCommandLineParser parser;
    parser.setApplicationDescription("Linux/X11 application for the Logitech Spotlight device.");
    const QCommandLineOption versionOption(QStringList{ "v", "version"}, "Print application version.");
    const QCommandLineOption fullVersionOption(QStringList{ "f", "fullversion" });
    const QCommandLineOption helpOption(QStringList{ "h", "help"}, "Show command line usage.");
    const QCommandLineOption commandOption(QStringList{ "c", "command"}, "Send command to a running instance.", "cmd");
    parser.addOptions({versionOption, helpOption, commandOption, fullVersionOption});

    QStringList args;
    for(int i = 0; i < argc; ++i) {
      args.push_back(argv[i]);
    }
    parser.process(args);
    if (parser.isSet(helpOption))
    {
      print() << QCoreApplication::applicationName().toStdString() << " "
              << projecteur::version_string() << std::endl;
      print() << "Usage: projecteur [option]" << std::endl;
      print() << "<Options>";
      print() << "  -h, --help             " << helpOption.description().toStdString();
      print() << "  -v, --version          " << versionOption.description().toStdString();
      print() << "  -c COMMAND             " << commandOption.description().toStdString() << std::endl;
      print() << "<Commands>";
      print() << "  spot=[on|off]          Turn spotlight on/off.";
      print() << "  settings=[show|hide]   Show/hide preferences dialog.";
      print() << "  quit                   Quit the running instance.";
      return 0;
    }
    else if (parser.isSet(versionOption) || parser.isSet(fullVersionOption))
    {
      print() << QCoreApplication::applicationName().toStdString() << " "
              << projecteur::version_string();
      if (parser.isSet(fullVersionOption) ||
          (std::string(projecteur::version_branch()) != "master" && 
           std::string(projecteur::version_branch()) != "not-within-git-repo"))
      { // Not a build from master branch, print out additional information:
        print() << "  - git-branch: " << projecteur::version_branch();
        print() << "  - git-hash: " << projecteur::version_fullhash();
      }
      // Show if we have a build from modified sources
      if (projecteur::version_isdirty())
        print() << "  - dirty-flag: " << projecteur::version_isdirty();
      return 0;
    }
    else if (parser.isSet(commandOption))
    {
      ipcCommand = parser.value(commandOption);
      if (ipcCommand.isEmpty()) {
        error() << "Command cannot be an empty string.";
        return 44;
      }
    }
  }

  RunGuard guard(QCoreApplication::applicationName());
  if (!guard.tryToRun())
  {
    if (ipcCommand.size())
    {
      return ProjecteurCommandClientApp(ipcCommand, argc, argv).exec();
    }
    else {
      error() << "Another application instance is already running. Exiting.";
      return 42;
    }
  }
  else if (ipcCommand.size())
  {
    // No other application instance running - but command option was used.
    error() << "Cannot send command '" << ipcCommand.toStdString() << "' - no running application instance found.";
    return 43;
  }

  //QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  ProjecteurApplication app(argc, argv);
  return app.exec();
}
