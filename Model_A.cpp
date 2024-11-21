
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <unistd.h>

class Observer {
public:
    virtual ~Observer() = default;

    virtual void update(int newValue) = 0;
};

class ResourceSubject {
private:
    int value = 0;
    std::unordered_set<std::shared_ptr<Observer>> observers;
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> running {true};

public:
    void attach(std::shared_ptr<Observer> observer) {
        std::unique_lock<std::mutex> lock(mtx);
        observers.insert(observer);
    }

    void detach(std::shared_ptr<Observer> observer) {
        std::unique_lock<std::mutex> lock(mtx);
        observers.erase(observer);
    }

    void notify() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.notify_all();
    }

    int getValue() const {
        std::unique_lock<std::mutex> lock(mtx);
        return value;
    }

    void setValue(int newValue) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            value = newValue;
        }
        notify();
    }

    void stop() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            running = false;
        }
        notify();
    }

    std::future<int> asyncGetValue() {
        return std::async([this] {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { 
                return !running || this->value != 0; 
            });
            return value;
        });
    }
};

class SpecificObserver : public Observer {
private:
    const std::string name;

public:
    SpecificObserver(const std::string& name_) : name(name_) {}

    void update(int newValue) override {
        std::cout << name << ": Resource updated to " << newValue << std::endl;
    }
};

int main() {
    std::shared_ptr<ResourceSubject> resourceSubject = std::make_shared<ResourceSubject>();

    // Create and attach two observers using std::shared_ptr
    std::shared_ptr<SpecificObserver> observer1 = std::make_shared<SpecificObserver>("Observer 1");
    std::shared_ptr<SpecificObserver> observer2 = std::make_shared<SpecificObserver>("Observer 2");

    resourceSubject->attach(observer1);
    resourceSubject->attach(observer2);

    // Run observers asynchronously
    std::thread thread1([&] {
        while (true) {
            auto future = resourceSubject->asyncGetValue();
            int updatedValue = future.get();
            observer1->update(updatedValue);
        }
    });

    std::thread thread2([&] {
        while (true) {
            auto future = resourceSubject->asyncGetValue();
            int updatedValue = future.get();
            observer2->update(updatedValue);
        }
    });

    // Simulate resource updates and program termination
    sleep(2);
    resourceSubject->setValue(10);
    sleep(1);
    resourceSubject->setValue(20);
    sleep(1);

    resourceSubject->stop();

    thread1.join();
    thread2.join();

    return 0;
}  
