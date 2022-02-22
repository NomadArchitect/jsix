#pragma once
/// \file channel.h
/// Definition of channel objects and related functions

#include <j6/caps.h>
#include <util/bip_buffer.h>
#include <util/counted.h>
#include <util/spinlock.h>

#include "objects/kobject.h"

namespace obj {

/// Channels are uni-directional means of sending data
class channel :
    public kobject
{
public:
    /// Capabilities on a newly constructed channel handle
    constexpr static j6_cap_t creation_caps = j6_cap_channel_all;

    channel();
    virtual ~channel();

    static constexpr kobject::type type = kobject::type::channel;

    /// Check if the channel has space for a message to be sent
    inline bool can_send() const { return m_can_send; }

    /// Check if the channel has a message wiating already
    inline bool can_receive() const { return m_can_recv; }

    /// Put a message into the channel
    /// \arg data Buffer of data to write
    /// \returns  The number of bytes successfully written
    size_t enqueue(const util::buffer &data);

    /// Get a message from the channel, copied into a provided buffer
    /// \arg buffer  The buffer to copy data into
    /// \returns     The number of bytes copied into the provided buffer
    size_t dequeue(util::buffer buffer);

    /// Mark this channel as closed, all future calls to enqueue or
    /// dequeue messages will fail with j6_status_closed.
    void close();

    /// Check if this channel has been closed.
    inline bool closed() const { return m_closed; }

private:
    size_t m_len;
    uintptr_t m_data;
    bool m_closed;
    bool m_can_send;
    bool m_can_recv;
    util::bip_buffer m_buffer;
    util::spinlock m_close_lock;
};

} // namespace obj
