/*
Copyright (c) 1995-2021, The Regents of the University of California
through the Lawrence Berkeley National Laboratory and the
International Computer Science Institute. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

(1) Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

(2) Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

(3) Neither the name of the University of California, Lawrence Berkeley
    National Laboratory, U.S. Dept. of Energy, International Computer
    Science Institute, nor the names of contributors may be used to endorse
    or promote products derived from this software without specific prior
    written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/


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
