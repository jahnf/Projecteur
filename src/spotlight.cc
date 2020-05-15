// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "spotlight.h"

#include "virtualdevice.h"
#include "logging.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <QVarLengthArray>

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <unistd.h>
#include <vector>

LOGGING_CATEGORY(device, "device")

// Function declaration to check for extra devices, defintion in generated source
bool isExtraDeviceSupported(quint16 vendorId, quint16 productId);
QString getExtraDeviceName(quint16 vendorId, quint16 productId);

namespace {
  // -----------------------------------------------------------------------------------------------
  // List of supported devices
  const std::vector<Spotlight::SupportedDevice> supportedDefaultDevices {
    {0x46d, 0xc53e, false, "Logitech Spotlight (USB)"},
    {0x46d, 0xb503, true, "Logitech Spotlight (Bluetooth)"},
  };

  // -----------------------------------------------------------------------------------------------
  bool isDeviceSupported(quint16 vendorId, quint16 productId)
  {
    const auto it = std::find_if(supportedDefaultDevices.cbegin(), supportedDefaultDevices.cend(),
    [vendorId, productId](const Spotlight::SupportedDevice& d) {
      return (vendorId == d.vendorId) && (productId == d.productId);
    });
    return (it != supportedDefaultDevices.cend()) || isExtraDeviceSupported(vendorId, productId);
  }

  // -----------------------------------------------------------------------------------------------
  bool isAdditionallySupported(quint16 vendorId, quint16 productId, const QList<Spotlight::SupportedDevice>& devices)
  {
    const auto it = std::find_if(devices.cbegin(), devices.cend(),
    [vendorId, productId](const Spotlight::SupportedDevice& d) {
      return (vendorId == d.vendorId) && (productId == d.productId);
    });
    return (it != devices.cend());
  }

  // -----------------------------------------------------------------------------------------------
  // Return the defined device name for vendor/productId if defined in
  // any of the supported device lists (default, extra, additional)
  QString getUserDeviceName(quint16 vendorId, quint16 productId, const QList<Spotlight::SupportedDevice>& additionalDevices)
  {
    const auto it = std::find_if(supportedDefaultDevices.cbegin(), supportedDefaultDevices.cend(),
    [vendorId, productId](const Spotlight::SupportedDevice& d) {
      return (vendorId == d.vendorId) && (productId == d.productId);
    });
    if (it != supportedDefaultDevices.cend() && it->name.size()) return it->name;

    auto extraName = getExtraDeviceName(vendorId, productId);
    if (!extraName.isEmpty()) return extraName;

    const auto ait = std::find_if(additionalDevices.cbegin(), additionalDevices.cend(),
    [vendorId, productId](const Spotlight::SupportedDevice& d) {
      return (vendorId == d.vendorId) && (productId == d.productId);
    });
    if (ait != additionalDevices.cend() && ait->name.size()) return ait->name;
    return QString();
  }

  // -----------------------------------------------------------------------------------------------
  quint16 readUShortFromDeviceFile(const QString& filename)
  {
    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
      return f.readAll().trimmed().toUShort(nullptr, 16);
    }
    return 0;
  }

  // -----------------------------------------------------------------------------------------------
  quint64 readULongLongFromDeviceFile(const QString& filename)
  {
    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
      return f.readAll().trimmed().toULongLong(nullptr, 16);
    }
    return 0;
  }

  // -----------------------------------------------------------------------------------------------
  QString readStringFromDeviceFile(const QString& filename)
  {
    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
      return f.readAll().trimmed();
    }
    return QString();
  }

  // -----------------------------------------------------------------------------------------------
  QString readPropertyFromDeviceFile(const QString& filename, const QString& property)
  {
    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
      auto contents = f.readAll();
      QTextStream in(&contents, QIODevice::ReadOnly);
      while (!in.atEnd())
      {
        const auto line = in.readLine();
        if (line.startsWith(property) && line.size() > property.size() && line[property.size()] == '=')
        {
          return line.mid(property.size() + 1);
        }
      }
    }
    return QString();
  }

  // -----------------------------------------------------------------------------------------------
  Spotlight::Device deviceFromUEventFile(const QString& filename)
  {
    QFile f(filename);
    Spotlight::Device spotlightDevice;
    static const QString hid_id("HID_ID");
    static const QString hid_name("HID_NAME");
    static const QString hid_phys("HID_PHYS");
    static const std::array<const QString*, 3> properties = { &hid_id, &hid_name, &hid_phys };

    if (!f.open(QIODevice::ReadOnly)) {
      return spotlightDevice;
    }

    auto contents = f.readAll();
    QTextStream in(&contents, QIODevice::ReadOnly);
    while (!in.atEnd())
    {
      const auto line = in.readLine();
      for (const auto property : properties)
      {
        if (line.startsWith(property) && line.size() > property->size() && line[property->size()] == '=')
        {
          const QString value = line.mid(property->size() + 1);

          if (property == hid_id)
          {
            const auto ids = value.split(':');
            const auto busType = ids.size() ? ids[0].toUShort(nullptr, 16) : 0;
            switch (busType)
            {
              case BUS_USB: spotlightDevice.busType = Spotlight::Device::BusType::Usb; break;
              case BUS_BLUETOOTH: spotlightDevice.busType = Spotlight::Device::BusType::Bluetooth; break;
            }
            spotlightDevice.id.vendorId = ids.size() > 1 ? ids[1].toUShort(nullptr, 16) : 0;
            spotlightDevice.id.productId = ids.size() > 2 ? ids[2].toUShort(nullptr, 16) : 0;
          }
          else if (property == hid_name)
          {
            spotlightDevice.name = value;
          }
          else if (property == hid_phys)
          {
            spotlightDevice.id.phys = value.split('/').first();
          }
        }
      }
    }
    return spotlightDevice;
  }
} // --- end anonymous namespace

// -------------------------------------------------------------------------------------------------
Spotlight::Spotlight(QObject* parent, Options options)
  : QObject(parent)
  , m_options(std::move(options))
  , m_activeTimer(new QTimer(this))
  , m_connectionTimer(new QTimer(this))
{
  m_activeTimer->setSingleShot(true);
  m_activeTimer->setInterval(600);

  connect(m_activeTimer, &QTimer::timeout, this, [this](){
    m_spotActive = false;
    emit spotActiveChanged(false);
  });

  if (m_options.enableUInput) {
    m_virtualDevice = VirtualDevice::create();
  }
  else {
    logInfo(device) << tr("Virtual device initialization was skipped.");
  }

  m_connectionTimer->setSingleShot(true);
  // From detecting a change from inotify, the device needs some time to be ready for open
  // TODO: This interval seems to work, but it is arbitrary - there should be a better way.
  m_connectionTimer->setInterval(800);

  connect(m_connectionTimer, &QTimer::timeout, this, [this]() {
    logDebug(device) << tr("New connection check triggered");
    connectDevices();
  });

  // Try to find already attached device(s) and connect to it.
  connectDevices();
  setupDevEventInotify();
}

// -------------------------------------------------------------------------------------------------
Spotlight::~Spotlight() = default;

// -------------------------------------------------------------------------------------------------
bool Spotlight::anySpotlightDeviceConnected() const
{
  for (const auto& dc : m_deviceConnections)
  {
    for (const auto& c : dc.second.map) {
      if (c.second.notifier && c.second.notifier->isEnabled())
        return true;
    }
  }
  return false;
}

// -------------------------------------------------------------------------------------------------
uint32_t Spotlight::connectedDeviceCount() const
{
  uint32_t count = 0;
  for (const auto& dc : m_deviceConnections)
  {
    for (const auto& c : dc.second.map) {
      if (c.second.notifier && c.second.notifier->isEnabled()) {
        ++count; break;
      }
    }
  }
  return count;
}

// -------------------------------------------------------------------------------------------------
QList<Spotlight::ConnectedDeviceInfo> Spotlight::connectedDevices() const
{
  QList<ConnectedDeviceInfo> devices;
  for (const auto& dc : m_deviceConnections) {
    devices.push_back(ConnectedDeviceInfo{dc.first, dc.second.deviceName});
  }
  return devices;
}

// -------------------------------------------------------------------------------------------------
int Spotlight::connectDevices()
{
  const auto scanResult = scanForDevices(m_options.additionalDevices);
  for (const auto& dev : scanResult.devices)
  {
    // Get all readable event input device paths for the device
    QStringList inputPaths;
    std::for_each(dev.subDevices.cbegin(), dev.subDevices.cend(),
    [&inputPaths](const SubDevice& subDevice){
      if (subDevice.inputDeviceReadable) inputPaths.append(subDevice.inputDeviceFile);
    });
    if (inputPaths.empty()) continue; // TODO debug msg

    auto& connectionDetails = m_deviceConnections[dev.id];
    if (connectionDetails.deviceId != dev.id) {
      connectionDetails.deviceName = dev.userName.size() ? dev.userName : dev.name;
      connectionDetails.deviceId = dev.id;
    }

    for (const auto& inputPath : inputPaths)
    {
      auto find_it = connectionDetails.map.find(inputPath);

      // Check if a connection for the path exists...
      if (find_it != connectionDetails.map.end() && find_it->second.notifier
          && find_it->second.notifier->isEnabled()) {
        continue;
      }
      else {
        auto connection = openEventDevice(inputPath, dev);
        if (connection.notifier && connection.notifier->isEnabled())
        {
          const bool anyConnectedBefore = anySpotlightDeviceConnected();
          addInputEventHandler(connectionDetails, connection);
          connectionDetails.map[inputPath] = std::move(connection);
          if (connectionDetails.map.size() == 1)
          {
            logInfo(device) << tr("Connected device: %1 (%2:%3)")
                               .arg(connectionDetails.deviceName)
                               .arg(dev.id.vendorId, 4, 16, QChar('0'))
                               .arg(dev.id.productId, 4, 16, QChar('0'));
            emit deviceConnected(dev.id, connectionDetails.deviceName);
          }
          logDebug(device) << tr("Connected sub-device: %1 (%2:%3) %4")
                              .arg(connectionDetails.deviceName)
                              .arg(dev.id.vendorId, 4, 16, QChar('0'))
                              .arg(dev.id.productId, 4, 16, QChar('0'))
                              .arg(inputPath);
          emit subDeviceConnected(dev.id, connectionDetails.deviceName, inputPath);
          if (!anyConnectedBefore) emit anySpotlightDeviceConnectedChanged(true);
        }
        else {
          connectionDetails.map.erase(inputPath);
        }
      }
    }
    if (connectionDetails.map.empty()) {
      m_deviceConnections.erase(dev.id);
    }
  }
  return m_deviceConnections.size();
}

// -------------------------------------------------------------------------------------------------
Spotlight::DeviceConnection Spotlight::openEventDevice(const QString& devicePath, const Device& dev)
{
  const int evfd = ::open(devicePath.toLocal8Bit().constData(), O_RDONLY, 0);
  DeviceConnection connection(devicePath, ConnectionType::Event, ConnectionMode::ReadOnly);

  if (evfd < 0) {
    logDebug(device) << tr("Opening input event device failed:") << devicePath;
    return connection;
  }

  struct input_id id{};
  ioctl(evfd, EVIOCGID, &id); // get device id's

  connection.info.vendorId = id.vendor;
  connection.info.productId = id.product;

  // Check against given device id
  if ( id.vendor != dev.id.vendorId || id.product != dev.id.productId)
  {
    ::close(evfd);
    logDebug(device) << tr("Device id mismatch: %1 (%2:%3)")
                        .arg(devicePath)
                        .arg(id.vendor, 4, 16, QChar('0'))
                        .arg(id.product, 4, 16, QChar('0'));
    return connection;
  }

  unsigned long bitmask = 0;
  if (ioctl(evfd, EVIOCGBIT(0, sizeof(bitmask)), &bitmask) < 0)
  {
    ::close(evfd);
    logWarn(device) << tr("Cannot get device properties: %1 (%2:%3)")
                        .arg(devicePath)
                        .arg(id.vendor, 4, 16, QChar('0'))
                        .arg(id.product, 4, 16, QChar('0'));
    return connection;
  }

  connection.info.grabbed = [this, evfd, &devicePath]()
  {
    // Grab device inputs if a virtual device exists.
    if (m_virtualDevice)
    {
      const int res = ioctl(evfd, EVIOCGRAB, 1);
      if (res == 0) { return true; }

      // Grab not successful
      logError(device) << tr("Error grabbing device: %1 (return value: %2)").arg(devicePath).arg(res);
      ioctl(evfd, EVIOCGRAB, 0);
    }
    return false;
  }();

//  const bool hasRepEv = !!(bitmask & (1 << EV_REP));
//  const bool hasRelEv = !!(bitmask & (1 << EV_REL));
//  unsigned long relEvents = 0;
//  int len = ioctl(evfd, EVIOCGBIT(EV_REL, sizeof(relEvents)), &relEvents);
//  const bool hasRelXEvents = !!(relEvents & (1 << REL_X));
//  const bool hasRelYEvents = !!(relEvents & (1 << REL_Y));

  // Create socket notifier
  connection.notifier = std::make_unique<QSocketNotifier>(evfd, QSocketNotifier::Read);
  QSocketNotifier* const notifier = connection.notifier.get();
  // Auto clean up and close descriptor on destruction of notifier
  connect(notifier, &QSocketNotifier::destroyed, [grabbed = connection.info.grabbed, notifier]() {
    if (grabbed) {
      ioctl(static_cast<int>(notifier->socket()), EVIOCGRAB, 0);
    }
    ::close(static_cast<int>(notifier->socket()));
  });

  return connection;
}

// -------------------------------------------------------------------------------------------------
void Spotlight::removeDeviceConnection(const QString &devicePath)
{
  for (auto dc_it = m_deviceConnections.begin(); dc_it != m_deviceConnections.end(); )
  {
    auto& connMap = dc_it->second.map;
    auto find_it = connMap.find(devicePath);

    if (find_it != connMap.end())
    {
      logDebug(device) << tr("Disconnected sub-device: %1 (%2:%3) %4")
                          .arg(dc_it->second.deviceName).arg(dc_it->first.vendorId, 4, 16, QChar('0'))
                          .arg(dc_it->first.productId, 4, 16, QChar('0')).arg(devicePath);
      emit subDeviceDisconnected(dc_it->first, dc_it->second.deviceName, devicePath);
      connMap.erase(find_it);
    }

    if (connMap.empty())
    {
      logInfo(device) << tr("Disconnected device: %1 (%2:%3)")
                         .arg(dc_it->second.deviceName).arg(dc_it->first.vendorId, 4, 16, QChar('0'))
                         .arg(dc_it->first.productId, 4, 16, QChar('0'));
      emit deviceDisconnected(dc_it->first, dc_it->second.deviceName);
      dc_it = m_deviceConnections.erase(dc_it);
    }
    else {
      ++dc_it;
    }
  }
}

// -------------------------------------------------------------------------------------------------
void Spotlight::addInputEventHandler(ConnectionDetails& connDetails, DeviceConnection& connection)
{
  if (connection.info.type != ConnectionType::Event
      || !connection.notifier || !connection.notifier->isEnabled()) {
    return;
  }

  QSocketNotifier* const notifier = connection.notifier.get();
  connect(notifier, &QSocketNotifier::activated,
  [this, notifier, info = connection.info, id=connDetails.deviceId](int fd)
  {
    struct input_event ev;
    const auto sz = ::read(fd, &ev, sizeof(ev));

    if (sz == -1) // Error, e.g. if the usb device was unplugged...
    {
      notifier->setEnabled(false);

      if (!anySpotlightDeviceConnected()) {
        emit anySpotlightDeviceConnectedChanged(false);
      }

      QTimer::singleShot(0, this, [this, devicePath=info.devicePath](){
        removeDeviceConnection(devicePath);
      });

      return;
    }
    else if (sz != sizeof(ev))
    {
      return;
    }

    // -- Handle move events
    if (ev.type == EV_REL)
    {
      if (ev.code == REL_X || ev.code == REL_Y) // relative (mouse) move events
      {
        if (!m_activeTimer->isActive()) {
          m_spotActive = true;
          emit spotActiveChanged(true);
        }
        // Send the relative event as fake mouse movement
        if (m_virtualDevice) m_virtualDevice->emitEvent(ev); // TODO also send enclosing SYN event?
        m_activeTimer->start();
      }
      return;
    }



    // EV_MSC, MSC_SCAN is emitted for devices on a button press, before the EV_KEY event.
    // The value identifies the actual button on the device.
    //  - physical device buttons can be identified with this.

    logDebug(device) << tr("Device Event: %1 (%2:%3) | type=%4, code=%5 value=%6")
                        .arg(info.devicePath).arg(id.vendorId, 4, 16, QChar('0'))
                        .arg(id.productId, 4, 16, QChar('0'))
                        .arg(ev.type, 2, 16, QChar('0'))
                        .arg(ev.code, 4, 16, QChar('0'))
                        .arg(ev.value, 8, 16, QChar('0'));

    // time is the timestamp, it returns the time at which the event happened.
    // type is for example EV_REL for relative moment, EV_KEY for a keypress or release.
    //   More types are defined in include/linux/input-event-codes.h.
    // code is event code, for example REL_X or KEY_BACKSPACE,
    //   again a complete list is in include/linux/input-event-codes.h.
    // value is the value the event carries. Either a relative change for EV_REL,
    //   absolute new value for EV_ABS (joysticks ...),
    //   or 0 for EV_KEY for release, 1 for keypress and 2 for autorepeat.

    switch(ev.type)
    {
      case EV_REL: // On relative (mouse move) events, activate spotlight
        if (ev.code == REL_X || ev.code == REL_Y)
        {
          if (!m_activeTimer->isActive()) {
            m_spotActive = true;
            emit spotActiveChanged(true);
          }
          // Send the relative event as fake mouse movement
          if (m_virtualDevice) m_virtualDevice->emitEvent(ev);
          m_activeTimer->start();
        }
        break;

      case EV_KEY:
        if (info.grabbed)
        {
          // For now: pass event through to uinput (planned in v0.8: custom button/action mapping)
          if (m_virtualDevice) m_virtualDevice->emitEvent(ev);
        }
        break;

      default: {
        // TODO We don't need to forward EV_SYN events on mapped
        if (m_virtualDevice) m_virtualDevice->emitEvent(ev);
      }
    }
  });
}

// -------------------------------------------------------------------------------------------------
bool Spotlight::setupDevEventInotify()
{
  int fd = -1;
#if defined(IN_CLOEXEC)
  fd = inotify_init1(IN_CLOEXEC);
#endif
  if (fd == -1)
  {
    fd = inotify_init();
    if (fd == -1) {
      logError(device) << tr("inotify_init() failed. Detection of new attached devices will not work.");
      return false;
    }
  }
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  const int wd = inotify_add_watch(fd, "/dev/input", IN_CREATE | IN_DELETE);

  if (wd < 0) {
    logError(device) << tr("inotify_add_watch for /dev/input returned with failure.");
    return false;
  }

  const auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(notifier, &QSocketNotifier::activated, [this](int fd)
  {
    int bytesAvaibable = 0;
    if (ioctl(fd, FIONREAD, &bytesAvaibable) < 0 || bytesAvaibable <= 0) {
      return; // Error or no bytes available
    }
    QVarLengthArray<char, 2048> buffer(bytesAvaibable);
    const auto bytesRead = read(fd, buffer.data(), static_cast<size_t>(bytesAvaibable));
    const char* at = buffer.data();
    const char* const end = at + bytesRead;
    while (at < end)
    {
      const auto event = reinterpret_cast<const inotify_event*>(at);

      if ((event->mask & (IN_CREATE)) && QString(event->name).startsWith("event"))
      {
        // Trigger new device scan and connect if a new event device was created.
        m_connectionTimer->start();
      }

      at += sizeof(inotify_event) + event->len;
    }
  });

  connect(notifier, &QSocketNotifier::destroyed, [notifier]() {
    ::close(static_cast<int>(notifier->socket()));
  });
  return true;
}

// -------------------------------------------------------------------------------------------------
Spotlight::ScanResult Spotlight::scanForDevices(const QList<SupportedDevice>& additionalDevices)
{
  constexpr char hidDevicePath[] = "/sys/bus/hid/devices";

  ScanResult result;
  const QFileInfo dpInfo(hidDevicePath);

  if (!dpInfo.exists()) {
    result.errorMessages.push_back(tr("HID device path '%1' does not exist.").arg(hidDevicePath));
    return result;
  }

  if (!dpInfo.isExecutable()) {
    result.errorMessages.push_back(tr("HID device path '%1': Cannot list files.").arg(hidDevicePath));
    return result;
  }


  QDirIterator hidIt(hidDevicePath, QDir::System | QDir::Dirs | QDir::Executable | QDir::NoDotAndDotDot);
  while (hidIt.hasNext())
  {
    hidIt.next();

    const QFileInfo uEventFile(QDir(hidIt.filePath()).filePath("uevent"));
    if (!uEventFile.exists()) continue;

    // Get basic information from uevent file
    Device newDevice = deviceFromUEventFile(uEventFile.filePath());
    const auto& deviceId = newDevice.id;
    // Skip unsupported devices
    if (deviceId.vendorId == 0 || deviceId.productId == 0) continue;
    if (!isDeviceSupported(deviceId.vendorId, deviceId.productId)
        && !(isAdditionallySupported(deviceId.vendorId, deviceId.productId, additionalDevices))) continue;

    // Check if device is already in list (and we have another sub-device for it)
    const auto find_it = std::find_if(result.devices.begin(), result.devices.end(),
    [&newDevice](const Device& existingDevice){
      return existingDevice.id == newDevice.id;
    });

    Device& rootDevice = [&find_it, &result, &newDevice, &additionalDevices]() -> Device&
    {
      if (find_it == result.devices.end())
      {
        newDevice.userName = getUserDeviceName(newDevice.id.vendorId, newDevice.id.productId, additionalDevices);
        result.devices.push_back(newDevice);
        return result.devices.last();
      }
      return *find_it;
    }();

    SubDevice subDevice;

    // Iterate over 'input' sub-dircectory, check for input-hid device nodes
    const QFileInfo inputSubdir(QDir(hidIt.filePath()).filePath("input"));
    if (inputSubdir.exists() || inputSubdir.isExecutable())
    {
      QDirIterator inputIt(inputSubdir.filePath(), QDir::System | QDir::Dirs | QDir::Executable | QDir::NoDotAndDotDot);
      while (inputIt.hasNext())
      {
        inputIt.next();

        if (readUShortFromDeviceFile(QDir(inputIt.filePath()).filePath("id/vendor")) != rootDevice.id.vendorId
            || readUShortFromDeviceFile(QDir(inputIt.filePath()).filePath("id/product")) != rootDevice.id.productId)
        {
          logDebug(device) << tr("Input device vendor/product id mismatch.");
          break;
        }

        QDirIterator dirIt(inputIt.filePath(), QDir::System | QDir::Dirs | QDir::Executable | QDir::NoDotAndDotDot);
        while (dirIt.hasNext())
        {
          dirIt.next();
          if (!dirIt.fileName().startsWith("event")) continue;
          subDevice.inputDeviceFile = readPropertyFromDeviceFile(QDir(dirIt.filePath()).filePath("uevent"), "DEVNAME");
          if (!subDevice.inputDeviceFile.isEmpty()) {
            subDevice.inputDeviceFile = QDir("/dev").filePath(subDevice.inputDeviceFile);
            break;
          }
        }

        if (subDevice.inputDeviceFile.isEmpty()) continue;
        subDevice.phys = readStringFromDeviceFile(QDir(inputIt.filePath()).filePath("phys"));

        // Check if device supports relative events
        const auto supportedEvents = readULongLongFromDeviceFile(QDir(inputIt.filePath()).filePath("capabilities/ev"));
        const bool hasRelativeEvents = !!(supportedEvents & (1 << EV_REL));
        // Check if device supports relative x and y event types
        const auto supportedRelEv = readULongLongFromDeviceFile(QDir(inputIt.filePath()).filePath("capabilities/rel"));
        const bool hasRelXEvents = !!(supportedRelEv & (1 << REL_X));
        const bool hasRelYEvents = !!(supportedRelEv & (1 << REL_Y));

        subDevice.hasRelativeEvents = hasRelativeEvents && hasRelXEvents && hasRelYEvents;

        const QFileInfo fi(subDevice.inputDeviceFile);
        subDevice.inputDeviceReadable = fi.isReadable();
        subDevice.inputDeviceWritable = fi.isWritable();
        break;
      }
    }

    // Iterate over 'hidraw' sub-dircectory, check for hidraw device node
    const QFileInfo hidrawSubdir(QDir(hidIt.filePath()).filePath("hidraw"));
    if (hidrawSubdir.exists() || hidrawSubdir.isExecutable())
    {
      QDirIterator hidrawIt(hidrawSubdir.filePath(), QDir::System | QDir::Dirs | QDir::Executable | QDir::NoDotAndDotDot);
      while (hidrawIt.hasNext())
      {
        hidrawIt.next();
        if (!hidrawIt.fileName().startsWith("hidraw")) continue;
        subDevice.hidrawDeviceFile = readPropertyFromDeviceFile(QDir(hidrawIt.filePath()).filePath("uevent"), "DEVNAME");
        if (!subDevice.hidrawDeviceFile.isEmpty()) {
          subDevice.hidrawDeviceFile = QDir("/dev").filePath(subDevice.hidrawDeviceFile);
          const QFileInfo fi(subDevice.hidrawDeviceFile);
          subDevice.hidrawDeviceReadable = fi.isReadable();
          subDevice.hidrawDeviceWritable = fi.isWritable();
          break;
        }
      }
    }

    if (subDevice.inputDeviceFile.size() || subDevice.hidrawDeviceFile.size())
    {
      rootDevice.subDevices.push_back(subDevice);
    }
  }

  for (const auto& dev : result.devices)
  {
    const bool allReadable = std::all_of(dev.subDevices.cbegin(), dev.subDevices.cend(),
    [](const SubDevice& subDevice){
      return (subDevice.hidrawDeviceFile.isEmpty() || subDevice.hidrawDeviceReadable)
          && (subDevice.inputDeviceFile.isEmpty() || subDevice.inputDeviceReadable);
    });

    const bool allWriteable = std::all_of(dev.subDevices.cbegin(), dev.subDevices.cend(),
    [](const SubDevice& subDevice){
      return (subDevice.hidrawDeviceFile.isEmpty() || subDevice.hidrawDeviceWritable)
          && (subDevice.inputDeviceFile.isEmpty() || subDevice.inputDeviceWritable);
    });

    result.numDevicesReadable += allReadable;
    result.numDevicesWritable += allWriteable;
  }

  return result;
}
