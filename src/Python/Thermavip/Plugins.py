

class VipPluginInterface:
    """
    Base Plugin interface class
    
    A Python plugins must be located in a self contained folder within the 'VipPlugins' directory,    
    and the plugin name is considered to be this folder's name.
    
    The plugin folder must contain a __init__.py in order to be imported. This __init__.py
    must define at the module level an object which type inherits VipPluginInterface called 'PluginName_interface'
    """

    Success = 1      # Loading succeeded
    Failure = 2      # Loading failed
    ExitProcess = 3  # Loading succeeded and Thermavip should exit
    
    def load(self):
        """
        Load the plugin. This function is called only once at Thermavip startup.

        Returns
        -------
        Code : int
            load result, one of Success, Failure or ExitProcess.

        """
        return self.Success
    
    def unload(self):
        """
        Unload the plugin. This function is called only once at Thermavip exit.
        """
        pass
    
    def version(self):
        """
        Returns the plugin version as a string.

        """
        return str()
    
    def author(self):
        """
        Returns the plugin author.
        """
        return str()
    
    def link(self):
        """
        Returns a reference link for this plugin.
        """
        return str()
    
    def description(self):
        """
        Returns a small description of this plugin.
        """
        return str()
    
    def stylesheet(self):
        """
        Returns an additional Qt stylesheet that must be added to the global application stylesheet.
        """
        return str()
    
    def save(self):
        """
        Save the plugin state as a bytes object.
        """
        return bytes()
    
    def restore(self, state : bytes):
        """
        Restore the plugin state from a bytes object.
        """
        pass
        
    