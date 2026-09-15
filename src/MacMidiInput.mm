#include "MacMidiInput.h"

#include "CircuitStompProtocol.h"

#include <CoreMIDI/CoreMIDI.h>
#include <dispatch/dispatch.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace circuitpedal {
namespace {

std::string midiStatusError(const char* operation, OSStatus status)
{
    return std::string(operation) + " failed (OSStatus "
        + std::to_string(status) + ").";
}

struct MidiState final : std::enable_shared_from_this<MidiState> {
    MIDIClientRef client = 0;
    MIDIPortRef inputPort = 0;
    std::set<MIDIEndpointRef> connectedSources;
    ActionCallback actionCallback;
    DiagnosticCallback diagnosticCallback;
    std::atomic<bool> active { false };
    std::atomic<bool> reconciliationQueued { false };
    std::atomic<std::size_t> connectedSourceCount { 0U };

    void report(const std::string& message)
    {
        if (active.load(std::memory_order_acquire) && diagnosticCallback)
            diagnosticCallback(message);
    }

    void reconcileSources()
    {
        reconciliationQueued.store(false, std::memory_order_release);
        if (!active.load(std::memory_order_acquire) || inputPort == 0)
            return;

        std::set<MIDIEndpointRef> available;
        const ItemCount sourceCount = MIDIGetNumberOfSources();
        for (ItemCount index = 0; index < sourceCount; ++index)
        {
            const MIDIEndpointRef source = MIDIGetSource(index);
            if (source != 0)
                available.insert(source);
        }

        for (auto iterator = connectedSources.begin(); iterator != connectedSources.end();)
        {
            if (available.find(*iterator) != available.end())
            {
                ++iterator;
                continue;
            }
            (void)MIDIPortDisconnectSource(inputPort, *iterator);
            iterator = connectedSources.erase(iterator);
        }

        for (const MIDIEndpointRef source : available)
        {
            if (connectedSources.find(source) != connectedSources.end())
                continue;
            const OSStatus status = MIDIPortConnectSource(inputPort, source, nullptr);
            if (status == noErr)
                connectedSources.insert(source);
            else
                report(midiStatusError("Connect MIDI source", status));
        }
        connectedSourceCount.store(connectedSources.size(), std::memory_order_release);
    }

    void queueReconciliation()
    {
        if (!active.load(std::memory_order_acquire)
            || reconciliationQueued.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        const std::weak_ptr<MidiState> weakState = weak_from_this();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (const auto state = weakState.lock())
                state->reconcileSources();
        });
    }

    void queueAction(const ControllerAction& action)
    {
        const std::weak_ptr<MidiState> weakState = weak_from_this();
        const ControllerAction actionCopy = action;
        dispatch_async(dispatch_get_main_queue(), ^{
            const auto state = weakState.lock();
            if (!state || !state->active.load(std::memory_order_acquire))
                return;
            if (state->actionCallback)
                state->actionCallback(actionCopy);
        });
    }

    void decodePackets(const MIDIPacketList* packetList)
    {
        if (packetList == nullptr || !active.load(std::memory_order_acquire))
            return;

        const MIDIPacket* packet = &packetList->packet[0];
        for (UInt32 packetIndex = 0; packetIndex < packetList->numPackets; ++packetIndex)
        {
            std::size_t byteIndex = 0U;
            const std::size_t length = static_cast<std::size_t>(packet->length);
            while (byteIndex < length)
            {
                const std::uint8_t status = packet->data[byteIndex];
                if ((status & circuitstomp::prototype1::statusTypeMask)
                        == circuitstomp::prototype1::controlChangeStatus
                    && byteIndex + 2U < length)
                {
                    const auto action = circuitstomp::decodeMidi1Message(
                        status,
                        packet->data[byteIndex + 1U],
                        packet->data[byteIndex + 2U]);
                    if (action.has_value())
                        queueAction(*action);
                    byteIndex += 3U;
                }
                else
                {
                    ++byteIndex;
                }
            }
            packet = MIDIPacketNext(packet);
        }
    }

    void shutdown() noexcept
    {
        active.store(false, std::memory_order_release);
        reconciliationQueued.store(false, std::memory_order_release);
        if (inputPort != 0)
        {
            for (const MIDIEndpointRef source : connectedSources)
                (void)MIDIPortDisconnectSource(inputPort, source);
            connectedSources.clear();
            connectedSourceCount.store(0U, std::memory_order_release);
            (void)MIDIPortDispose(inputPort);
            inputPort = 0;
        }
        if (client != 0)
        {
            (void)MIDIClientDispose(client);
            client = 0;
        }
        actionCallback = {};
        diagnosticCallback = {};
    }
};

void midiReadCallback(const MIDIPacketList* packetList,
                      void* readProcRefCon,
                      void*)
{
    auto* state = static_cast<MidiState*>(readProcRefCon);
    if (state != nullptr)
        state->decodePackets(packetList);
}

void midiNotificationCallback(const MIDINotification*, void* refCon)
{
    auto* state = static_cast<MidiState*>(refCon);
    if (state != nullptr)
        state->queueReconciliation();
}

} // namespace

struct MacMidiInput::Impl {
    std::shared_ptr<MidiState> state;
};

MacMidiInput::MacMidiInput()
    : impl_(std::make_unique<Impl>())
{
}

MacMidiInput::~MacMidiInput()
{
    stop();
}

bool MacMidiInput::start(ActionCallback actionCallback,
                         DiagnosticCallback diagnosticCallback,
                         std::string& error)
{
    stop();
    error.clear();

    auto state = std::make_shared<MidiState>();
    state->actionCallback = std::move(actionCallback);
    state->diagnosticCallback = std::move(diagnosticCallback);

    OSStatus status = MIDIClientCreate(CFSTR("CircuitPedal CircuitStomp Input"),
                                       midiNotificationCallback,
                                       state.get(),
                                       &state->client);
    if (status != noErr)
    {
        error = midiStatusError("Create MIDI client", status);
        state->shutdown();
        return false;
    }

    status = MIDIInputPortCreate(state->client,
                                 CFSTR("CircuitStomp Input"),
                                 midiReadCallback,
                                 state.get(),
                                 &state->inputPort);
    if (status != noErr)
    {
        error = midiStatusError("Create MIDI input port", status);
        state->shutdown();
        return false;
    }

    state->active.store(true, std::memory_order_release);
    impl_->state = state;
    state->reconcileSources();
    return true;
}

void MacMidiInput::stop() noexcept
{
    if (!impl_ || !impl_->state)
        return;
    impl_->state->shutdown();
    impl_->state.reset();
}

bool MacMidiInput::running() const noexcept
{
    return impl_ && impl_->state
        && impl_->state->active.load(std::memory_order_acquire);
}

std::size_t MacMidiInput::connectedSourceCount() const noexcept
{
    return impl_ && impl_->state
        ? impl_->state->connectedSourceCount.load(std::memory_order_acquire)
        : 0U;
}

} // namespace circuitpedal
