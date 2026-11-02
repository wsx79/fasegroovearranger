// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#if JUCE_LINUX || JUCE_BSD
#include <unistd.h>
#endif
namespace fga {
inline bool verifyEventLoopLifetime(bool nested){
#if JUCE_LINUX || JUCE_BSD
    int first[2]{},second[2]{};
    if(::pipe(first)!=0)return false;
    if(::pipe(second)!=0){::close(first[0]);::close(first[1]);return false;}
    bool firstCalled=false,removedCalled=false,nestedCalled=false;
    juce::LinuxEventLoop::registerFdCallback(first[0],[&](int fd){char value;const auto n=::read(fd,&value,1);firstCalled=n==1;juce::LinuxEventLoop::unregisterFdCallback(second[0]);
        if(nested){juce::MessageManager::callAsync([&]{nestedCalled=true;});juce::MessageManager::getInstance()->runDispatchLoopUntil(2);}});
    juce::LinuxEventLoop::registerFdCallback(second[0],[&](int fd){char value;const auto n=::read(fd,&value,1);removedCalled=n==1;});
    const bool written=::write(first[1],"a",1)==1&&::write(second[1],"b",1)==1;
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    juce::LinuxEventLoop::unregisterFdCallback(first[0]);juce::LinuxEventLoop::unregisterFdCallback(second[0]);
    for(int fd:{first[0],first[1],second[0],second[1]})::close(fd);
    return written&&firstCalled&&!removedCalled&&(!nested||nestedCalled);
#else
    juce::ignoreUnused(nested);return true;
#endif
}
}
