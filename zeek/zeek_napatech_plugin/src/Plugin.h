
#ifndef ZEEK_PLUGIN_BRO_NAPATECH
#define ZEEK_PLUGIN_BRO_NAPATECH

#include <zeek/plugin/Plugin.h>

namespace plugin::Zeek_Napatech {

class Plugin : public zeek::plugin::Plugin
{
protected:
    // Overridden from plugin::Plugin.
	zeek::plugin::Configuration Configure();
};

extern Plugin plugin;

}
//}

#endif
