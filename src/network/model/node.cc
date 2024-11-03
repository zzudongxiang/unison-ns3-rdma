/*
 * Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: George F. Riley<riley@ece.gatech.edu>
 *          Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "node.h"

#include "application.h"
#include "net-device.h"
#include "node-list.h"
#include "packet.h"

#include "ns3/assert.h"
#include "ns3/boolean.h"
#include "ns3/global-value.h"
#include "ns3/log.h"
#include "ns3/object-vector.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Node");

NS_OBJECT_ENSURE_REGISTERED(Node);

/**
 * \relates Node
 * \anchor GlobalValueChecksumEnabled
 * \brief A global switch to enable all checksums for all protocols.
 */
static GlobalValue g_checksumEnabled =
    GlobalValue("ChecksumEnabled",
                "A global switch to enable all checksums for all protocols",
                BooleanValue(false),
                MakeBooleanChecker());

TypeId
Node::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Node")
            .SetParent<Object>()
            .SetGroupName("Network")
            .AddConstructor<Node>()
            .AddAttribute("DeviceList",
                          "The list of devices associated to this Node.",
                          ObjectVectorValue(),
                          MakeObjectVectorAccessor(&Node::m_devices),
                          MakeObjectVectorChecker<NetDevice>())
            .AddAttribute("ApplicationList",
                          "The list of applications associated to this Node.",
                          ObjectVectorValue(),
                          MakeObjectVectorAccessor(&Node::m_applications),
                          MakeObjectVectorChecker<Application>())
            .AddAttribute("Id",
                          "The id (unique integer) of this Node.",
                          TypeId::ATTR_GET, // allow only getting it.
                          UintegerValue(0),
                          MakeUintegerAccessor(&Node::m_id),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute(
                "SystemId",
                "The systemId of this node: a unique integer used for parallel simulations.",
                TypeId::ATTR_GET | TypeId::ATTR_SET,
                UintegerValue(0),
                MakeUintegerAccessor(&Node::m_sid),
                MakeUintegerChecker<uint32_t>());
    return tid;
}

Node::Node()
    : m_id(0),
      m_sid(0)
{
    NS_LOG_FUNCTION(this);
    Construct();
}

Node::Node(uint32_t sid)
    : m_id(0),
      m_sid(sid)
{
    NS_LOG_FUNCTION(this << sid);
    Construct();
}

void
Node::Construct()
{
    NS_LOG_FUNCTION(this);
    m_id = NodeList::Add(this);
}

Node::~Node()
{
    NS_LOG_FUNCTION(this);
}

uint32_t
Node::GetId() const
{
    return m_id;
}

Time
Node::GetLocalTime() const
{
    return Simulator::Now();
}

uint32_t
Node::GetSystemId() const
{
    return m_sid;
}

void
Node::SetSystemId(uint32_t systemId)
{
    NS_LOG_FUNCTION(this << systemId);
    m_sid = systemId;
}

uint32_t
Node::AddDevice(Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    uint32_t index = m_devices.size();
    m_devices.push_back(device);
    device->SetNode(this);
    device->SetIfIndex(index);
    device->SetReceiveCallback(MakeCallback(&Node::NonPromiscReceiveFromDevice, this));
    Simulator::ScheduleWithContext(GetId(), Seconds(0.0), &NetDevice::Initialize, device);
    NotifyDeviceAdded(device);
    return index;
}

Ptr<NetDevice>
Node::GetDevice(uint32_t index) const
{
    NS_ASSERT_MSG(index < m_devices.size(),
                  "Device index " << index << " is out of range (only have " << m_devices.size()
                                  << " devices).");
    return m_devices[index];
}

uint32_t
Node::GetNDevices() const
{
    return m_devices.size();
}

uint32_t
Node::AddApplication(Ptr<Application> application)
{
    NS_LOG_FUNCTION(this << application);
    uint32_t index = m_applications.size();
    m_applications.push_back(application);
    application->SetNode(this);
    Simulator::ScheduleWithContext(GetId(), Seconds(0.0), &Application::Initialize, application);
    return index;
}

Ptr<Application>
Node::GetApplication(uint32_t index) const
{
    NS_ASSERT_MSG(index < m_applications.size(),
                  "Application index " << index << " is out of range (only have "
                                       << m_applications.size() << " applications).");
    return m_applications[index];
}

uint32_t
Node::GetNApplications() const
{
    return m_applications.size();
}

void
Node::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_deviceAdditionListeners.clear();
    m_handlers.clear();
    for (auto i = m_devices.begin(); i != m_devices.end(); i++)
    {
        Ptr<NetDevice> device = *i;
        device->Dispose();
        *i = nullptr;
    }
    m_devices.clear();
    for (auto i = m_applications.begin(); i != m_applications.end(); i++)
    {
        Ptr<Application> application = *i;
        application->Dispose();
        *i = nullptr;
    }
    m_applications.clear();
    Object::DoDispose();
}

void
Node::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    for (auto i = m_devices.begin(); i != m_devices.end(); i++)
    {
        Ptr<NetDevice> device = *i;
        device->Initialize();
    }
    for (auto i = m_applications.begin(); i != m_applications.end(); i++)
    {
        Ptr<Application> application = *i;
        application->Initialize();
    }

    Object::DoInitialize();
}

void
Node::RegisterProtocolHandler(ProtocolHandler handler,
                              uint16_t protocolType,
                              Ptr<NetDevice> device,
                              bool promiscuous)
{
    NS_LOG_FUNCTION(this << &handler << protocolType << device << promiscuous);
    Node::ProtocolHandlerEntry entry;
    entry.handler = handler;
    entry.protocol = protocolType;
    entry.device = device;
    entry.promiscuous = promiscuous;

    // On demand enable promiscuous mode in netdevices
    if (promiscuous)
    {
        if (!device)
        {
            for (auto i = m_devices.begin(); i != m_devices.end(); i++)
            {
                Ptr<NetDevice> dev = *i;
                dev->SetPromiscReceiveCallback(MakeCallback(&Node::PromiscReceiveFromDevice, this));
            }
        }
        else
        {
            device->SetPromiscReceiveCallback(MakeCallback(&Node::PromiscReceiveFromDevice, this));
        }
    }

    m_handlers.push_back(entry);
}

void
Node::UnregisterProtocolHandler(ProtocolHandler handler)
{
    NS_LOG_FUNCTION(this << &handler);
    for (auto i = m_handlers.begin(); i != m_handlers.end(); i++)
    {
        if (i->handler.IsEqual(handler))
        {
            m_handlers.erase(i);
            break;
        }
    }
}

bool
Node::ChecksumEnabled()
{
    BooleanValue val;
    g_checksumEnabled.GetValue(val);
    return val.Get();
}

bool
Node::PromiscReceiveFromDevice(Ptr<NetDevice> device,
                               Ptr<const Packet> packet,
                               uint16_t protocol,
                               const Address& from,
                               const Address& to,
                               NetDevice::PacketType packetType)
{
    NS_LOG_FUNCTION(this << device << packet << protocol << &from << &to << packetType);
    return ReceiveFromDevice(device, packet, protocol, from, to, packetType, true);
}

bool
Node::NonPromiscReceiveFromDevice(Ptr<NetDevice> device,
                                  Ptr<const Packet> packet,
                                  uint16_t protocol,
                                  const Address& from)
{
    NS_LOG_FUNCTION(this << device << packet << protocol << &from);
    return ReceiveFromDevice(device,
                             packet,
                             protocol,
                             from,
                             device->GetAddress(),
                             NetDevice::PacketType(0),
                             false);
}

bool
Node::ReceiveFromDevice(Ptr<NetDevice> device,
                        Ptr<const Packet> packet,
                        uint16_t protocol,
                        const Address& from,
                        const Address& to,
                        NetDevice::PacketType packetType,
                        bool promiscuous)
{
    NS_LOG_FUNCTION(this << device << packet << protocol << &from << &to << packetType
                         << promiscuous);
    NS_ASSERT_MSG(Simulator::GetContext() == GetId(),
                  "Received packet with erroneous context ; "
                      << "make sure the channels in use are correctly updating events context "
                      << "when transferring events from one node to another.");
    bool found = false;

    for (auto i = m_handlers.begin(); i != m_handlers.end(); i++)
    {
        if (!i->device || (i->device == device))
        {
            if (i->protocol == 0 || i->protocol == protocol)
            {
                if (promiscuous == i->promiscuous)
                {
                    i->handler(device, packet, protocol, from, to, packetType);
                    found = true;
                }
            }
        }
    }
    NS_LOG_DEBUG("Node " << GetId() << " ReceiveFromDevice:  dev " << device->GetIfIndex()
                         << " (type=" << device->GetInstanceTypeId().GetName() << ") Packet UID "
                         << packet->GetUid() << " handler found: " << found);
    return found;
}

void
Node::RegisterDeviceAdditionListener(DeviceAdditionListener listener)
{
    NS_LOG_FUNCTION(this << &listener);
    m_deviceAdditionListeners.push_back(listener);
    // and, then, notify the new listener about all existing devices.
    for (auto i = m_devices.begin(); i != m_devices.end(); ++i)
    {
        listener(*i);
    }
}

void
Node::UnregisterDeviceAdditionListener(DeviceAdditionListener listener)
{
    NS_LOG_FUNCTION(this << &listener);
    for (auto i = m_deviceAdditionListeners.begin(); i != m_deviceAdditionListeners.end(); i++)
    {
        if ((*i).IsEqual(listener))
        {
            m_deviceAdditionListeners.erase(i);
            break;
        }
    }
}

void
Node::NotifyDeviceAdded(Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    for (auto i = m_deviceAdditionListeners.begin(); i != m_deviceAdditionListeners.end(); i++)
    {
        (*i)(device);
    }
}

uint32_t
Node::GetNodeType()
{
    return m_node_type;
}

bool
Node::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader& ch)
{
    NS_ASSERT_MSG(false,
                  "Calling SwitchReceiveFromDevice() on a non-switch node or this function is not "
                  "implemented");
}

void
Node::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p)
{
    NS_ASSERT_MSG(
        false,
        "Calling NotifyDequeue() on a non-switch node or this function is not implemented");
}

} // namespace ns3
