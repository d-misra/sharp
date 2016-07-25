#include "LockedData.hpp"
#include "FakeMutex.hpp"
using namespace std;
using namespace sharp;

int main() {
    auto fake_mutex = FakeMutex{};
    __attribute__((unused)) auto object = 1;
    assert(fake_mutex.lock_state == FakeMutex::LockState::UNLOCKED);
    {
        auto proxy = LockedData<int, FakeMutex>::UniqueLockedProxy{object,
            fake_mutex};
        assert(fake_mutex.lock_state == FakeMutex::LockState::LOCKED);
        assert(proxy.operator->() == &object);
        assert(*proxy == 1);
        assert(&(*proxy) == &object);
    }
    assert(fake_mutex.lock_state == FakeMutex::LockState::UNLOCKED);

    // const unique locked proxy shuold read lock the lock
    assert(fake_mutex.lock_state == FakeMutex::LockState::UNLOCKED);
    {
        auto proxy = LockedData<int, FakeMutex>::ConstUniqueLockedProxy{object,
            fake_mutex};
        assert(fake_mutex.lock_state == FakeMutex::LockState::SHARED);
        assert(proxy.operator->() == &object);
        assert(*proxy == 1);
        assert(&(*proxy) == &object);
    }
    assert(fake_mutex.lock_state == FakeMutex::LockState::UNLOCKED);

    return 0;
}