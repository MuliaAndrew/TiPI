#include <atomic>
#include <memory>

class MCSLock {
    struct alignas(64) LinkedSpinLock {
      LinkedSpinLock* next;
      unsigned locked;
    };
    std::atomic<std::shared_ptr<LinkedSpinLock>> top;
    std::shared_ptr<LinkedSpinLock> my;
    std::shared_ptr<LinkedSpinLock> pred;

  public:
    void lock() {
      my = std::make_shared<LinkedSpinLock>();
      
    }
};