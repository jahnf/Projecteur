// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QMetaObject>
#include <QPointer>

#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
#include <QCoreApplication>
#include <QEvent>
#endif

#include <functional>
#include <type_traits>

namespace async {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
// Invoke a (lambda) function for context QObject with queued connection.
template <typename F>
void invoke(QObject* context, F function) {
  QMetaObject::invokeMethod(context, std::move(function), Qt::QueuedConnection);
}
#else
// ... older Qt versions < 5.10
namespace detail {
template <typename F>
struct FEvent : public QEvent {
  using Fun = typename std::decay<F>::type;
  Fun fun;
  FEvent(Fun && fun) : QEvent(QEvent::None), fun(std::move(fun)) {}
  FEvent(const Fun & fun) : QEvent(QEvent::None), fun(fun) {}
  ~FEvent() { fun(); }
}; }

template <typename F>
void invoke(QObject* context, F&& function) {
  QCoreApplication::postEvent(context, new detail::FEvent<F>(std::forward<F>(function)));
}
#endif


// --- Helpers to deduce std::function type from a lambda.
template <typename>
struct remove_member;

template <typename C, typename T>
struct remove_member<T C::*> {
  using type = T;
};

template <typename C, typename R, typename... Args>
struct remove_member<R (C::*)(Args...) const> {
  using type = R(Args...);
};

/// Create a safe function object, guaranteed to be invoked in the context of
/// the given QObject context.
template <typename R, typename... Args>
std::function<R(Args...)> makeSafeCallback(QObject* context,
                                           std::function<R(Args...)> f,
                                           bool forceQueued = true)
{
  QPointer<QObject> ctxPtr(context);
  std::function<R(Args...)> res =
  [ctxPtr, forceQueued, func = std::move(f)](Args... args) {
    // Check if context object is still valid
    if (ctxPtr.isNull()) {
      return;
    }

    #if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    QMetaObject::invokeMethod(ctxPtr,
      std::bind(std::move(func), std::forward<Args>(args)...),
      forceQueued ? Qt::QueuedConnection : Qt::AutoConnection);
    // Note: if forceQueued is false and current thread is the same as
    // the context thread -> execute directly
    #else
    // For Qt < 5.10 the call is always queued via the event queue
    async::invoke(ctxPtr, std::bind(std::move(func), std::forward<Args>(args)...));
    #endif
  };
  return res;
}

template <typename F>
auto makeSafeCallback(QObject* context, F f, bool forceQueued = true) {
  using ft = decltype(&F::operator());
  std::function<typename remove_member<ft>::type> func = std::move(f);
  return async::makeSafeCallback(context, std::move(func), forceQueued);
}

/// Deriving from this class will makeSafeCallback and postSelf methods for QObject based
/// classes available.
///
/// Example:
/// @code
/// class MyClass : public QObject, public async::Async<MyClass> {
///     Q_OBJECT
///     // ... implementation..
/// }
/// @endcode
template <typename T>
class Async
{
protected:
  /// Returns a function object that is guaranteed to be invoked in the own thread context.
  template <typename F>
  auto makeSafeCallback(F f) {
    return async::makeSafeCallback(static_cast<T*>(this), std::move(f));
  }

  /// Post a function to the own event loop.
  template <typename F>
  void postSelf(F function) {
    async::invoke(static_cast<T*>(this), std::move(function));
  }

public:
  /// Post a task to the object's event loop.
  template <typename Task>
  void postTask(Task task) {
    postSelf(std::move(task));
  }

  template<typename Task>
  static constexpr bool is_void_return_v = std::is_same<std::result_of_t<Task()>, void>::value;

  /// Post a task with no return value and provide a callback.
  template <typename Task, typename Callback>
  typename std::enable_if_t<is_void_return_v<Task>>
  postCallback(Task task, Callback callback)
  {
    auto wrapper = [task = std::move(task), callback = std::move(callback)]() mutable
    {
      task();
      callback();
    };
    postSelf(std::move(wrapper));
  }

  /// Post a task with return value and a callback that takes the return value
  /// as an argument.
  template <typename Task, typename Callback>
  typename std::enable_if_t<!is_void_return_v<Task>>
  postCallback(Task task, Callback callback)
  {
    auto wrapper = [task = std::move(task), callback = std::move(callback)]() mutable {
      callback(task());
    };
    postSelf(std::move(wrapper));
  }
};

} // end namespace async
