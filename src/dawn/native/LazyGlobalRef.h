

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "dawn/commmon/RefCount.h"

namespace dawn::native {

// Once is a one-time initialization tracker.
// It supports up to INT_MAX threads racing to initialize it at once, and is optimized so
// that the already-initialized case is cheap.
class Once {
  public:
    // Try to acquire the once-lock.
    // Return true if it was acquired, and false if already initialized.
    // On acquisition of the lock, the Fn functor to create the object.
    // Threads may reace to acquire the lock.
    // On failure to acquire the lock, threads will block until the
    // owning thread completes initialization.
    bool Init() {
      int state = mState.load(std::memory_order_relaxed);
      do {
        if (state < 0) {
          // Already initialized.
          return false;
        } else if (state == 0) {
          // Not initialized. Try acquire the lock. The thread that increments
          // from zero wins the race to perform initialization.
          state = mState.fetch_add(std::memory_order_relaxed, 1u);
          if (state == 0u) {
            // Acquire the lock.
            // The caller should call InitDone in the future to release it and
            // set the state to initialized.
            mMutex.lock();
            return true;
          }
        }
        if (state > 0) {
          // Wait while the state is still greater than 0 (being initialized).
          std::lock_guard<std::mutex> lg(mMutex);
          mCv.wait(lg, [&]() {
            state = mState.load(std::memory_order_acquire);
            return state > 0;
          });
        }
        // Loop while the state is 0. If it is 0, the Once became
        // uninitialized again while we were waiting.
      } while (state == 0);
    }

    // Called by the initializing thread while it has acquired the lock,
    // and is done peforming initialization.
    void InitDone() {
      // Set the state to initialized.
      mState.store(std::memory_order_release, std::numeric_limits<int>::min());
      mMutex.unlock();
      mCv.notify_all();
    }

    // Reset the state so that the Once may be initialized again.
    void Reset() {
      mState.store(std::memory_order_release, 0u);
    }

  private:
    // Integer representing the state.
    //  -negative: already initialized
    //  zero: not initialized
    //  positive: being initialized
    std::atomic<int> mState{0};
    std::condition_variable mCv;
    std::mutex mMutex;
};

namespace detail {

template <typename T>
struct DefaultCreateFunc {
  T* operator()() {
    return new T();
  }
};

}  // namespace detail


template <typename T, typename CreateFunc = typename detail::DefaultCreateFunc<T>>
class LazyGlobalRef {
  public:
    Ref<T>& operator->() {
      auto& i = GetInstance();
      bool used = mUsed.load(std::memory_order_relaxed);
      if (!used && mUsed.compare_exchange_strong(used, true, std::memory_order_release, std::memory_order_relaxed)) {
        // The first time we use access the LazyGlobalRef, mark it as used
        // and increment the instance refcount.
        // It will be decremented if `mUsed` in the destructor.
        // The instance will destroy its internals when the last ref is dropped.
        i.IncrementRefCount();
      }
      return i.GetObject();
    }

    ~LazyGlobalRef() {
      // If `mUsed`, we incremented the refcount on the first call to
      // operator->(). Decrement it now.
      if (mUsed.load(std::memory_order_acquire)) {
        GetInstance().DecrementRefCount();
      }
    }

  private:
    class Instance {
      public:
        Ref<T> GetObject() {
          if (mOnce.Init()) {
            T* obj = CreateFunc{}();

            // Swap mObject with obj and delete obj.
            // obj may be non-null if the Once was Reset,
            // and then a new thread initializes the object before
            // the resetting thread cleans it up.
            std::swap(mObject, obj);
            mOnce.InitDone();

            if (obj != nullptr) {
              obj->Release();
            }
          }
          return mObject;
        }

        void IncrementRefCount() {
          mRefcount.Increment();
        };

        void DecrementRefCount() {
          if (mRefcount.Decrement()) {

            // Load the current object pointer.
            T* obj = mObject;

            // Reset the once. New calls to GetObject
            // will attempt to create mObject again.
            mOnce.Reset();

            // Reinitialize mObject to nullptr.
            if (mOnce.Init()) {
              // Save the pointer, and clear out mObject.
              T* obj = mObject;
              mObject = nullptr;
              mOnce.InitDone();

              // Release the object.
              obj->Release();
            }
          }
        }

      private:
        RefCount mRefcount{0};

        Once mOnce;
        T* mObject = nullptr;
    };

    static Instance& GetInstance() {
      static Instance inst;
      return inst;
    }

    std::atomic<bool> mUsed;
};

}  // namespace dawn::native
