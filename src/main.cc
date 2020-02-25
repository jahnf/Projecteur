// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "projecteurapp.h"
#include "projecteur-GitVersion.h"

#include "logging.h"
#include "runguard.h"
#include "settings.h"

#include <QCommandLineParser>

#ifndef NDEBUG
#include <QQmlDebuggingEnabler>
#endif

#include <iostream>
#include <iomanip>

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(x) #x

LOGGING_CATEGORY(appMain, "main")

namespace {
  class Main : public QObject {};

  std::ostream& operator<<(std::ostream& os, const QString& s) {
    os << s.toStdString();
    return os;
  }

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
  ProjecteurApplication::Options options;
  QStringList ipcCommands;
  {
    QCommandLineParser parser;
    parser.setApplicationDescription(Main::tr("Linux/X11 application for the Logitech Spotlight device."));
    const QCommandLineOption versionOption(QStringList{ "v", "version"}, Main::tr("Print application version."));
    const QCommandLineOption fullVersionOption(QStringList{ "f", "fullversion" });
    const QCommandLineOption helpOption(QStringList{ "h", "help"}, Main::tr("Show command line usage."));
    const QCommandLineOption fullHelpOption(QStringList{ "help-all"}, Main::tr("Show complete command line usage with all properties."));
    const QCommandLineOption cfgFileOption(QStringList{ "cfg" }, Main::tr("Set custom config file."), "file");
    const QCommandLineOption commandOption(QStringList{ "c", "command"}, Main::tr("Send command/property to a running instance."), "cmd");
    const QCommandLineOption deviceInfoOption(QStringList{ "d", "device-scan"}, Main::tr("Print device-scan results."));
    const QCommandLineOption logLvlOption(QStringList{ "l", "log-level" }, Main::tr("Set log level (dbg,inf,wrn,err)."), "lvl");
    const QCommandLineOption disableUInputOption(QStringList{ "disable-uinput" }, Main::tr("Disable uinput support."));
    const QCommandLineOption showDlgOnStartOption(QStringList{ "show-dialog" }, Main::tr("Show preferences dialog on start."));
    const QCommandLineOption dialogMinOnlyOption(QStringList{ "m", "minimize-only" }, Main::tr("Only allow minimizing the dialog."));
    const QCommandLineOption disableOverlayOption(QStringList{ "disable-overlay" }, Main::tr("Disable spotlight overlay completely."));
    const QCommandLineOption additionalDeviceOption(QStringList{ "D", "additional-device"},
                               Main::tr("Additional accepted device; DEVICE = vendorId:productId\n"
                                        "                         "
                                        "e.g., -D 04b3:310c; e.g. -D 0x0c45:0x8101"), "device");

    parser.addOptions({versionOption, helpOption, fullHelpOption, commandOption,
                       cfgFileOption, fullVersionOption, deviceInfoOption, logLvlOption,
                       disableUInputOption, showDlgOnStartOption, dialogMinOnlyOption,
                       disableOverlayOption, additionalDeviceOption});

    const QStringList args = [argc, &argv]()
    {
      const QStringList qtAppKeyValueOptions = {
        "-platform", "-platformpluginpath", "-platformtheme", "-plugin", "-display"
      };
      const QStringList qtAppSingleOptions = {"-reverse"};
      QStringList args;
      for (int i = 0; i < argc; ++i)
      { // Skip some default arguments supported by QtGuiApplication, we don't want to parse them
        // but they will get passed through to the ProjecteurApp.
        if (qtAppKeyValueOptions.contains(argv[i])) { ++i; }
        else if (qtAppSingleOptions.contains(argv[i])) { continue; }
        else { args.push_back(argv[i]); }
      }
      return args;
    }();

    parser.process(args);
    if (parser.isSet(helpOption) || parser.isSet(fullHelpOption))
    {
      print() << QCoreApplication::applicationName() << " "
              << projecteur::version_string() << std::endl;
      print() << "Usage: projecteur [option]" << std::endl;
      print() << "<Options>";
      print() << "  -h, --help             " << helpOption.description();
      print() << "  --help-all             " << fullHelpOption.description();
      print() << "  -v, --version          " << versionOption.description();
      print() << "  --cfg FILE             " << cfgFileOption.description();
      print() << "  -d, --device-scan      " << deviceInfoOption.description();
      print() << "  -l, --log-level LEVEL  " << logLvlOption.description();
      print() << "  -D DEVICE              " << additionalDeviceOption.description();
      if (parser.isSet(fullHelpOption)) {
        print() << "  --disable-uinput       " << disableUInputOption.description();
        print() << "  --show-dialog          " << showDlgOnStartOption.description();
        print() << "  -m, --minimize-only    " << dialogMinOnlyOption.description();
      }
      print() << "  -c COMMAND|PROPERTY    " << commandOption.description() << std::endl;
      print() << "<Commands>";
      print() << "  spot=[on|off]          " << Main::tr("Turn spotlight on/off.");
      print() << "  settings=[show|hide]   " << Main::tr("Show/hide preferences dialog.");
      if (parser.isSet(fullHelpOption)) {
        print() << "  preset=NAME            " << Main::tr("Set a preset.");
      }
      print() << "  quit                   " << Main::tr("Quit the running instance.");

      // Early return if the user not explicitly requested the full help
      if (!parser.isSet(fullHelpOption)) return 0;

      print() << "\n" << "<Properties>";
      // Helper function to get the range of valid values for a string property
      const auto getValues = [](const Settings::StringProperty& sp) -> QString
      {
        if (sp.type == Settings::StringProperty::Type::Integer
            || sp.type == Settings::StringProperty::Type::Double) {
          return QString("(%1 ... %2)").arg(sp.range[0].toString(), sp.range[1].toString());
        }
        else if (sp.type == Settings::StringProperty::Type::Bool) {
          return "(false, true)";
        }
        else if (sp.type == Settings::StringProperty::Type::Color) {
          return "(HTML-color; #RRGGBB)";
        }
        else if (sp.type == Settings::StringProperty::Type::StringEnum) {
          QStringList values;
          for (const auto& v : sp.range) {
            values.push_back(v.toString());
          }
          return QString("(%1)").arg(values.join(", "));
        }
        return QString();
      };

      int maxPropertyStringLength = 0;

      const std::vector<std::pair<QString, QString>> propertiesList =
        [getValues=std::move(getValues), &maxPropertyStringLength]()
        {
          std::vector<std::pair<QString, QString>> list;
          // Fill temporary list with properties to be able to format our output better
          Settings settings; // <-- FIXME unnecessary Settings instance
          for (const auto& sp : settings.stringProperties())
          {
            list.emplace_back(
              QString("%1=[%2]").arg(sp.first, sp.second.typeToString(sp.second.type)),
              getValues(sp.second));

            maxPropertyStringLength = qMax(maxPropertyStringLength, list.back().first.size());
          }
          return list;
        }();

      for (const auto& sp : propertiesList) {
        print() << "  " << std::left << std::setw(maxPropertyStringLength + 3) << sp.first << sp.second;
      }

      return 0;
    }

    if (parser.isSet(additionalDeviceOption)) {
      for (auto& deviceValue : parser.values(additionalDeviceOption)) {
        const auto devAttribs = deviceValue.split(":");
        const auto vendorId = devAttribs[0].toUShort(nullptr, 16);
        const auto productId = devAttribs[1].toUShort(nullptr, 16);
        if (vendorId == 0 || productId == 0) {
          error() << Main::tr("Invalid vendor/productId pair: ") << deviceValue;
        } else {
          const QString name = (devAttribs.size() >= 3) ? devAttribs[2] : "";
          options.additionalDevices.push_back({vendorId, productId, false, name});
        }
      }
    }

    if (parser.isSet(versionOption) || parser.isSet(fullVersionOption))
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

      // Additional useful information
      if (parser.isSet(fullVersionOption))
      {
        print() << "  - compiler: " << XSTRINGIFY(CXX_COMPILER_ID) << " "
                                    << XSTRINGIFY(CXX_COMPILER_VERSION);
        print() << "  - qt-version: (build: " << QT_VERSION_STR << ", runtime: " << qVersion() << ")";

        const auto result = DeviceScan::getDevices(options.additionalDevices);
        print() << "  - device-scan: "
                << QString("(errors: %1, devices: %2 [readable: %3, writable: %4])")
                   .arg(result.errorMessages.size()).arg(result.devices.size())
                   .arg(result.numDevicesReadable).arg(result.numDevicesWritable);
      }
      return 0;
    }

    if (parser.isSet(deviceInfoOption))
    {
      const auto result = DeviceScan::getDevices(options.additionalDevices);
      print() << QCoreApplication::applicationName() << " "
              << projecteur::version_string() << "; " << Main::tr("device scan") << std::endl;

      for (const auto& errmsg : result.errorMessages) {
        print() << "** " << Main::tr("Error: ") << errmsg;
      }

      print() << (result.errorMessages.size() ? "\n" : "")
              << Main::tr(" * Found %1 supported devices. (%2 readable, %3 writable)")
                 .arg(result.devices.size()).arg(result.numDevicesReadable).arg(result.numDevicesWritable);

      const auto busTypeToString = [](DeviceScan::Device::BusType type) -> QString {
        if (type == DeviceScan::Device::BusType::Usb) return "USB";
        if (type == DeviceScan::Device::BusType::Bluetooth) return "Bluetooth";
        return "unknown";
      };

      for (const auto& device : result.devices)
      {
        print() << "\n"
                << " +++ " << "name:     '" << device.name << "'";
        if (!device.userName.isEmpty()) {
          print() << "     " << "userName: '" << device.userName << "'";
        }

        const QStringList subDeviceList = [&device](){
          QStringList subDeviceList;
          for (const auto& sd: device.subDevices) {
            if (sd.deviceFile.size()) subDeviceList.push_back(sd.deviceFile);
          }
          return subDeviceList;
        }();

        const bool allReadable = std::all_of(device.subDevices.cbegin(), device.subDevices.cend(),
        [](const auto& subDevice){
          return subDevice.deviceReadable;
        });

        const bool allWriteable = std::all_of(device.subDevices.cbegin(), device.subDevices.cend(),
        [](const auto& subDevice){
          return subDevice.deviceWritable;
        });

        print() << "     " << "vendorId:  " << QString("%1").arg(device.id.vendorId, 4, 16, QChar('0'));
        print() << "     " << "productId: " << QString("%1").arg(device.id.productId, 4, 16, QChar('0'));
        print() << "     " << "phys:      " << device.id.phys;
        print() << "     " << "busType:   " << busTypeToString(device.busType);
        print() << "     " << "devices:   " << subDeviceList.join(", ");
        print() << "     " << "readable:  " << (allReadable ? "true" : "false");
        print() << "     " << "writable:  " << (allWriteable ? "true" : "false");
      }
      return 0;
    }
    else if (parser.isSet(commandOption))
    {
      ipcCommands = parser.values(commandOption);
      for (auto& value : ipcCommands) {
        value = value.trimmed();
      }
      ipcCommands.removeAll("");

      if (ipcCommands.isEmpty()) {
        error() << Main::tr("Command/Properties cannot be an empty string.");
        return 44;
      }
    }

    if (parser.isSet(cfgFileOption)) {
      options.configFile = parser.value(cfgFileOption);
    }

    options.enableUInput = !parser.isSet(disableUInputOption);
    options.showPreferencesOnStart = parser.isSet(showDlgOnStartOption);
    options.dialogMinimizeOnly = parser.isSet(dialogMinOnlyOption);
    options.disableOverlay = parser.isSet(disableOverlayOption);

    if (parser.isSet(logLvlOption)) {
      const auto lvl = logging::levelFromName(parser.value(logLvlOption));
      if (lvl != logging::level::unknown) {
        logging::setCurrentLevel(lvl);
      } else {
        error() << Main::tr("Cannot set log level, unknown level: '%1'").arg(parser.value(logLvlOption));
      }
    }
  }

  RunGuard guard(QCoreApplication::applicationName());
  if (!guard.tryToRun())
  {
    if (ipcCommands.size()) {
      return ProjecteurCommandClientApp(ipcCommands, argc, argv).exec();
    }
    error() << Main::tr("Another application instance is already running. Exiting.");
    return 42;
  }
  else if (ipcCommands.size())
  {
    // No other application instance running - but command option was used.
    logInfo(appMain) << Main::tr("Cannot send commands '%1' - no running application instance found.").arg(ipcCommands.join("; "));
    logWarning(appMain) << Main::tr("Cannot send commands '%1' - no running application instance found.").arg(ipcCommands.join("; "));
    error() << Main::tr("Cannot send commands '%1' - no running application instance found.").arg(ipcCommands.join("; "));
    return 43;
  }

  //QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  ProjecteurApplication app(argc, argv, options);
  return app.exec();
}
