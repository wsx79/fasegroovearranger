# JUCE 8.0.9 snapshots FD callbacks before dispatch. A preceding callback may
# unregister another target, and nested dispatch overwrites the shared snapshot.
# Keep each dispatch frame independent and skip registrations removed/replaced
# during an earlier callback (notably VST3 RunLoop destruction on project open).
if(CMAKE_SYSTEM_NAME MATCHES "Linux|FreeBSD")
    get_target_property(fga_events_modules juce::juce_events INTERFACE_JUCE_MODULE_PATH)
    set(fga_events "${fga_events_modules}/juce_events/native/juce_Messaging_linux.cpp")
    file(READ "${fga_events}" fga_events_text)
    string(REPLACE "\r\n" "\n" fga_events_text "${fga_events_text}")
    set(fga_dispatch_old [=[        callbackStorage.clear();
        getFunctionsToCallThisTime (callbackStorage);

        // CriticalSection should be available during the callback
        for (auto& fn : callbackStorage)
            (*fn)();

        return ! callbackStorage.empty();]=])
    set(fga_dispatch_new [=[        // FGA: each nested dispatch owns its callback snapshot.
        std::vector<SharedCallback> functions;
        getFunctionsToCallThisTime (functions);

        for (auto& fn : functions)
        {
            const auto stillRegistered = [&]
            {
                const ScopedLock sl (lock);
                return std::any_of (callbacks.begin(), callbacks.end(),
                                    [&] (const auto& entry) { return entry.second == fn; });
            }();

            // The previous callback may have destroyed a VST3 RunLoop target.
            // Do not hold the registration lock while invoking user code.
            if (stillRegistered)
                (*fn)();
        }

        return ! functions.empty();]=])
    string(FIND "${fga_events_text}" "${fga_dispatch_new}" fga_dispatch_applied)
    if(fga_dispatch_applied EQUAL -1)
        string(FIND "${fga_events_text}" "${fga_dispatch_old}" fga_dispatch_found)
        if(fga_dispatch_found EQUAL -1)
            message(FATAL_ERROR "Review the JUCE Linux pending-event lifetime adaptation")
        endif()
        string(REPLACE "${fga_dispatch_old}" "${fga_dispatch_new}" fga_events_text "${fga_events_text}")
        string(REPLACE "    std::vector<SharedCallback> callbackStorage;\n" "" fga_events_text "${fga_events_text}")
        file(WRITE "${fga_events}" "${fga_events_text}")
    endif()
endif()
