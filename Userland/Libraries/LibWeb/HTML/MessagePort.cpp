/*
 * Copyright (c) 2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/DOM/EventDispatcher.h>
#include <LibWeb/HTML/EventHandler.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/HTML/MessageEvent.h>
#include <LibWeb/HTML/MessagePort.h>

namespace Web::HTML {

JS_DEFINE_ALLOCATOR(MessagePort);

JS::NonnullGCPtr<MessagePort> MessagePort::create(JS::Realm& realm)
{
    return realm.heap().allocate<MessagePort>(realm, realm);
}

MessagePort::MessagePort(JS::Realm& realm)
    : DOM::EventTarget(realm)
{
}

MessagePort::~MessagePort() = default;

void MessagePort::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    set_prototype(&Bindings::ensure_web_prototype<Bindings::MessagePortPrototype>(realm, "MessagePort"_fly_string));
}

void MessagePort::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_remote_port);
}

// https://html.spec.whatwg.org/multipage/web-messaging.html#message-ports:transfer-steps
WebIDL::ExceptionOr<void> MessagePort::transfer_steps(HTML::TransferDataHolder&)
{
    // 1. Set value's has been shipped flag to true.
    m_has_been_shipped = true;

    // FIXME: 2. Set dataHolder.[[PortMessageQueue]] to value's port message queue.
    // FIXME: Support delivery of messages that haven't been delivered yet on the other side

    // 3. If value is entangled with another port remotePort, then:
    if (is_entangled()) {
        // 1. Set remotePort's has been shipped flag to true.
        m_remote_port->m_has_been_shipped = true;

        // 2. Set dataHolder.[[RemotePort]] to remotePort.
        // FIXME: Append an IPC::File to the dataHolder
    }

    // 4. Otherwise, set dataHolder.[[RemotePort]] to null.
    else {
        // FIXME: Note in the dataHolder that there are no fds
    }

    return {};
}

WebIDL::ExceptionOr<void> MessagePort::transfer_receiving_steps(HTML::TransferDataHolder const&)
{
    // 1. Set value's has been shipped flag to true.
    m_has_been_shipped = true;

    // FIXME 2. Move all the tasks that are to fire message events in dataHolder.[[PortMessageQueue]] to the port message queue of value,
    //     if any, leaving value's port message queue in its initial disabled state, and, if value's relevant global object is a Window,
    //     associating the moved tasks with value's relevant global object's associated Document.

    // FIXME: 3. If dataHolder.[[RemotePort]] is not null, then entangle dataHolder.[[RemotePort]] and value.
    //     (This will disentangle dataHolder.[[RemotePort]] from the original port that was transferred.)

    return {};
}

void MessagePort::disentangle()
{
    m_remote_port->m_remote_port = nullptr;
    m_remote_port = nullptr;
}

// https://html.spec.whatwg.org/multipage/web-messaging.html#entangle
void MessagePort::entangle_with(MessagePort& remote_port)
{
    if (m_remote_port.ptr() == &remote_port)
        return;

    // 1. If one of the ports is already entangled, then disentangle it and the port that it was entangled with.
    if (is_entangled())
        disentangle();
    if (remote_port.is_entangled())
        remote_port.disentangle();

    // 2. Associate the two ports to be entangled, so that they form the two parts of a new channel.
    //    (There is no MessageChannel object that represents this channel.)
    remote_port.m_remote_port = this;
    m_remote_port = &remote_port;
}

// https://html.spec.whatwg.org/multipage/web-messaging.html#dom-messageport-postmessage
void MessagePort::post_message(JS::Value message)
{
    // 1. Let targetPort be the port with which this MessagePort is entangled, if any; otherwise let it be null.
    auto* target_port = m_remote_port.ptr();

    // FIXME: 2. Let options be «[ "transfer" → transfer ]».

    // 3. Run the message port post message steps providing targetPort, message and options.

    // https://html.spec.whatwg.org/multipage/web-messaging.html#message-port-post-message-steps

    // FIXME: 1. Let transfer be options["transfer"].

    // FIXME: 2. If transfer contains this MessagePort, then throw a "DataCloneError" DOMException.

    // 3. Let doomed be false.
    bool doomed = false;

    // FIXME: 4. If targetPort is not null and transfer contains targetPort, then set doomed to true and optionally report to a developer console that the target port was posted to itself, causing the communication channel to be lost.

    // FIXME: 5. Let serializeWithTransferResult be StructuredSerializeWithTransfer(message, transfer). Rethrow any exceptions.

    // 6. If targetPort is null, or if doomed is true, then return.
    if (!target_port || doomed)
        return;

    // FIXME: 7. Add a task that runs the following steps to the port message queue of targetPort:

    // FIXME: This is an ad-hoc hack implementation instead, since we don't currently
    //        have serialization and deserialization of messages.
    main_thread_event_loop().task_queue().add(HTML::Task::create(HTML::Task::Source::PostedMessage, nullptr, [target_port, message] {
        MessageEventInit event_init {};
        event_init.data = message;
        event_init.origin = "<origin>"_string;
        target_port->dispatch_event(MessageEvent::create(target_port->realm(), HTML::EventNames::message, event_init));
    }));
}

void MessagePort::start()
{
    // FIXME: Message ports are supposed to be disabled by default.
}

// https://html.spec.whatwg.org/multipage/web-messaging.html#dom-messageport-close
void MessagePort::close()
{
    // 1. Set this MessagePort object's [[Detached]] internal slot value to true.
    set_detached(true);

    // 2. If this MessagePort object is entangled, disentangle it.
    if (is_entangled())
        disentangle();
}

#undef __ENUMERATE
#define __ENUMERATE(attribute_name, event_name)                         \
    void MessagePort::set_##attribute_name(WebIDL::CallbackType* value) \
    {                                                                   \
        set_event_handler_attribute(event_name, value);                 \
    }                                                                   \
    WebIDL::CallbackType* MessagePort::attribute_name()                 \
    {                                                                   \
        return event_handler_attribute(event_name);                     \
    }
ENUMERATE_MESSAGE_PORT_EVENT_HANDLERS(__ENUMERATE)
#undef __ENUMERATE

}
