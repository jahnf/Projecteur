#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>

class QGlobalShortcutX11 final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(QKeySequence key READ key WRITE setKey)

public:
    explicit QGlobalShortcutX11(QObject* parent = nullptr);
    explicit QGlobalShortcutX11(const QKeySequence& keyseq, QObject* parent = nullptr);
    virtual ~QGlobalShortcutX11() override;

    bool nativeEventFilter(const QByteArray& event_type, void* message, long* result) override;

    QKeySequence key() const;
    void setKey(const QKeySequence& keyseq);

signals:
    void activated();

private:
    bool m_enabled = false;
    QKeySequence m_keySeq;
    quint8 m_keyCode = 0;
    quint16 m_keyMods = 0;
    void unsetKey();
};
