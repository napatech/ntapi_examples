import ZeekControl.plugin
import ZeekControl.config

class Napatech(ZeekControl.plugin.Plugin):
    def __init__(self):
        super(Napatech, self).__init__(apiversion=1)

    def name(self):
        return 'napatech'

    def pluginVersion(self):
        return 1
    
    def init(self):

        # Use this plugin only if there is a Napatech interface in use
        for nn in self.nodes():
            if nn.type == 'worker' and nn.interface.startswith('napatech::'):
                return True
        
        return False
    
    def nodeKeys(self):

        script = ''
        return script
    
    def options(self):

        script = ''
        return script

    def zeekctl_config(self):

        script = ''       
        return script
