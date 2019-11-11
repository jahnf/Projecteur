#pragma once

#include <QObject>
#include <QSharedMemory>
#include <QSystemSemaphore>

class RunGuard
{
public:
  explicit RunGuard(const QString& key);
  ~RunGuard();

  bool isAnotherRunning();
  bool tryToRun();
  void release();

private:
  const QString m_key;
  const QString m_memLockKey;
  const QString m_sharedmemKey;

  QSharedMemory m_sharedMem;
  QSystemSemaphore m_memLock;

  Q_DISABLE_COPY(RunGuard)
};
