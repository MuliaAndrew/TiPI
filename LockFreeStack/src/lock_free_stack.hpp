#include <atomic>
#include <iostream>
#include <memory>

template <typename T>
struct Node {
    T data;
    Node* next;

    Node(T val) : data(val), next(nullptr) {}
};

template <typename T>
struct alignas(16) TaggedPointer {
    Node<T>* ptr;
    uintptr_t tag;

    TaggedPointer(Node<T>* p = nullptr, uintptr_t t = 0) : ptr(p), tag(t) {}
};

template <typename T>
class LockFreeStack {
    std::atomic<TaggedPointer<T>> head;

  public:
    LockFreeStack() : head(TaggedPointer<T>()) {}

    ~LockFreeStack() {
      while (pop());
    }

    void push(T value) {
      Node<T>* newNode = new Node<T>(value);
      TaggedPointer<T> oldHead = head.load(std::memory_order_relaxed);

      while (true) {
        newNode->next = oldHead.ptr; 
        TaggedPointer<T> newHead(newNode, oldHead.tag + 1);

        if (head.compare_exchange_weak(
          oldHead, 
          newHead,
          std::memory_order_release,
          std::memory_order_relaxed)) {
          break;
        }
      }
    }

    std::unique_ptr<T> pop() {
      TaggedPointer<T> oldHead = head.load(std::memory_order_relaxed);

      while (oldHead.ptr) {
        TaggedPointer<T> newHead(oldHead.ptr->next, oldHead.tag + 1);

        if (head.compare_exchange_weak(
          oldHead, newHead,
          std::memory_order_acquire,
          std::memory_order_relaxed)) {
          std::unique_ptr<T> result = std::make_unique<T>(oldHead.ptr->data);
          delete oldHead.ptr;
          return result;
        }
      }

      return nullptr; // Stack is empty
    }

    bool isEmpty() const {
        return head.load(std::memory_order_relaxed).ptr == nullptr;
    }
};
