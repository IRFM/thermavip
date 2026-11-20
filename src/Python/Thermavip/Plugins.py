"""Plugin entry-point conventions and lifecycle hooks for Thermavip."""



class VipPluginInterface:
    """Base class for a plugin's module-level interface object.

    Place the plugin package under VipPlugins. Its directory name is the plugin
    name. In its __init__.py, expose an instance of your subclass as
    <PluginName>_interface. For a package named Camera, for example, the entry
    point is Camera_interface.

    Override load() for initialization, unload() for cleanup, and the metadata or
    state hooks that apply to the plugin.
    """

    Success = 1      # Loading succeeded
    Failure = 2      # Loading failed
    ExitProcess = 3  # Loading succeeded and Thermavip should exit
    
    def load(self):
        """Initialize the plugin at startup. Return Success (the default), Failure, or
        ExitProcess to request exit after successful loading.
        """
        return self.Success
    
    def unload(self):
        """Release plugin resources at application exit. The base implementation takes
        no action.
        """
        pass
    
    def version(self):
        """Return the plugin version string. Defaults to an empty string."""
        return str()
    
    def author(self):
        """Return the plugin author as a string. Defaults to an empty string."""
        return str()
    
    def link(self):
        """Return a reference URL for the plugin. Defaults to an empty string."""
        return str()
    
    def description(self):
        """Return a short description of the plugin. Defaults to an empty string."""
        return str()
    
    def stylesheet(self):
        """Return Qt stylesheet text to add to the application stylesheet. Defaults to
        an empty string.
        """
        return str()
    
    def save(self):
        """Return the plugin state as bytes for a later restore(). Defaults to empty
        bytes.
        """
        return bytes()
    
    def restore(self, state : bytes):
        """Restore plugin state from the supplied bytes. The base implementation takes
        no action.
        """
        pass
        
    