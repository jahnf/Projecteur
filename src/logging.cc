// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "logging.h"

#include <QDateTime>
#include <QList>
#include <QMetaMethod>
#include <QPlainTextEdit>
#include <QPointer>
#include <QString>

#include <iostream>

namespace {
  // -----------------------------------------------------------------------------------------------
  void projecteurLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msgQString);
  void categoryFilterInfo(QLoggingCategory *category);

  // Install our custom message handler, store previous message handler
  const QtMessageHandler defaultMessageHandler = qInstallMessageHandler(projecteurLogHandler);
  const QLoggingCategory::CategoryFilter defaultCategoryFilter = QLoggingCategory::installFilter(categoryFilterInfo);
  QLoggingCategory::CategoryFilter currentCategoryFilter = categoryFilterInfo;

  constexpr char categoryPrefix[] = "projecteur.";
  inline bool isAppCategory(QLoggingCategory* category) {
    return (qstrncmp(categoryPrefix, category->categoryName(), sizeof(categoryPrefix)-1) == 0);
  }

  void categoryFilterDebug(QLoggingCategory *category)
  {
    if (isAppCategory(category))
    {
      category->setEnabled(QtDebugMsg, true);
      category->setEnabled(QtInfoMsg, true);
      category->setEnabled(QtWarningMsg, true);
      category->setEnabled(QtCriticalMsg, true);
    } else {
      defaultCategoryFilter(category);
    }
  }

  void categoryFilterInfo(QLoggingCategory *category)
  {
    if (isAppCategory(category)) {
      category->setEnabled(QtDebugMsg, false);
      category->setEnabled(QtInfoMsg, true);
      category->setEnabled(QtWarningMsg, true);
      category->setEnabled(QtCriticalMsg, true);
    } else {
      defaultCategoryFilter(category);
    }
  }

  void categoryFilterWarning(QLoggingCategory *category)
  {
    if (isAppCategory(category)) {
      category->setEnabled(QtDebugMsg, false);
      category->setEnabled(QtInfoMsg, false);
      category->setEnabled(QtWarningMsg, true);
      category->setEnabled(QtCriticalMsg, true);
    } else {
      defaultCategoryFilter(category);
    }
  }

  void categoryFilterError(QLoggingCategory *category)
  {
    if (isAppCategory(category))
    {
      category->setEnabled(QtDebugMsg, false);
      category->setEnabled(QtInfoMsg, false);
      category->setEnabled(QtWarningMsg, false);
      category->setEnabled(QtCriticalMsg, true);
    } else {
      defaultCategoryFilter(category);
    }
  }

  // -----------------------------------------------------------------------------------------------
  QPointer<QPlainTextEdit> logPlainTextEdit;
  QMetaMethod logAppendMetaMethod;
  QList<QString> logPlainTextCache; // log messages are stored here until a text edit is registered
  constexpr int logPlainTextCacheMax = 1000;

  // -----------------------------------------------------------------------------------------------
  void logToTextEdit(const QString& logMsg)
  {
    if (logPlainTextEdit) {
      logAppendMetaMethod.invoke(logPlainTextEdit, Qt::QueuedConnection, Q_ARG(QString, logMsg));
    } else if (logPlainTextCache.size() < logPlainTextCacheMax) {
      logPlainTextCache.push_back(logMsg);
    }
  }

  // -----------------------------------------------------------------------------------------------
  inline const char* typeToShortString(QtMsgType type) {
    switch (type) {
      case QtDebugMsg: return "dbg";
      case QtInfoMsg: return "inf";
      case QtWarningMsg: return "wrn";
      case QtCriticalMsg: return "err";
      case QtFatalMsg: return "fat";
    }
    return "";
  }

  // -----------------------------------------------------------------------------------------------
  // Currently all logging is done from within the Qt Gui thread
  // - if that changes and multiple threads will log, this needs a serious overhaul - NOT thread safe
  void projecteurLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msgQString)
  {
    // const char *file = context.file ? context.file : "";
    // const char *function = context.function ? context.function : "";
    const char *category = context.category ? context.category : "";

    #if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
      constexpr auto dateFormat = Qt::ISODateWithMs;
    #else
      constexpr auto dateFormat = Qt::ISODate;
    #endif

    const auto logMsg = QString("[%1][%2][%3] %4").arg(QDateTime::currentDateTime().toString(dateFormat),
                                                       typeToShortString(type), category, msgQString);

    if (type == QtDebugMsg || type == QtInfoMsg)
      std::cout << qUtf8Printable(logMsg) << std::endl;
    else
      std::cerr << qUtf8Printable(logMsg) << std::endl;

    logToTextEdit(logMsg);
  }
} // end anonymous namespace

namespace logging {
  void registerTextEdit(QPlainTextEdit* textEdit)
  {
    logPlainTextEdit = textEdit;
    if (!logPlainTextEdit) return;

    const auto index = logPlainTextEdit->metaObject()->indexOfMethod("appendPlainText(QString)");
    logAppendMetaMethod = logPlainTextEdit->metaObject()->method(index);

    for (const auto& logMsg : logPlainTextCache) {
      logAppendMetaMethod.invoke(logPlainTextEdit, Qt::QueuedConnection, Q_ARG(QString, logMsg));
    }

    logPlainTextCache.clear();
  }

  const char* levelToString(level lvl)
  {
    switch (lvl) {
      case level::debug: return "debug";
      case level::info: return "info";
      case level::warning: return "warning";
      case level::error: return "error";
      case level::custom: return "default/custom";
      case level::unknown: return "unknwon";
    }
    return "";
  }

  level levelFromName(const QString& name)
  {
    if (name == "dbg" || name == "debug") return level::debug;
    if (name == "inf" || name == "info") return level::info;
    if (name == "wrn" || name == "warning") return level::warning;
    if (name == "err" || name == "error") return level::error;
    return level::unknown;
  }

  level currentLevel()
  {
    if (currentCategoryFilter == defaultCategoryFilter) return level::custom;
    if (currentCategoryFilter == categoryFilterDebug) return level::debug;
    if (currentCategoryFilter == categoryFilterInfo) return level::info;
    if (currentCategoryFilter == categoryFilterWarning) return level::warning;
    if (currentCategoryFilter == categoryFilterError) return level::error;
    return level::unknown;
  }

  void setCurrentLevel(level lvl)
  {
    if (lvl == level::debug)
      QLoggingCategory::installFilter(categoryFilterDebug);
    else if (lvl == level::info)
      QLoggingCategory::installFilter(categoryFilterInfo);
    else if (lvl == level::warning)
      QLoggingCategory::installFilter(categoryFilterWarning);
    else if (lvl == level::error)
      QLoggingCategory::installFilter(categoryFilterError);
    else if (lvl == level::custom)
      QLoggingCategory::installFilter(defaultCategoryFilter);
  }

}
