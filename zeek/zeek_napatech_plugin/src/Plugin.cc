#include "Plugin.h"
#include "Napatech.h"
#include <pcap.h>
#include "iosource/pcap/Source.h"
#include "iosource/BPF_Program.h"
#include "iosource/Component.h"
#include "iosource/IOSource.h"
#include "iosource/Manager.h"
#include "iosource/Packet.h"
#include "iosource/PktDumper.h"
#include "iosource/PktSrc.h"


namespace plugin::Zeek_Napatech { Plugin plugin; }

using namespace plugin::Zeek_Napatech;

zeek::plugin::Configuration Plugin::Configure()
{
    AddComponent(new ::zeek::iosource::PktSrcComponent("NapatechReader", "napatech", ::zeek::iosource::PktSrcComponent::LIVE, ::zeek::iosource::pktsrc::NapatechSource::InstantiateNapatech));

    zeek::plugin::Configuration config;
    config.name = "Zeek::Napatech";
    config.description = "Packet acquisition via Napatech NTAPI";
    config.version.major = 1;
    config.version.minor = 0;
    config.version.patch = 0;

    return config;
}
