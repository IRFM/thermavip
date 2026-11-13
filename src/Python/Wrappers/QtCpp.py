import sys
import builtins
from importlib import import_module



def qobject_address(obj) -> int:
    """Return the native address of a live PyQt or PySide QObject."""
    for binding, runtime in (
        ("PyQt6", "PyQt6.sip"),
        ("PyQt5", "PyQt5.sip"),
        ("PySide6", "shiboken6"),
        ("PySide2", "shiboken2"),
    ):
        # An existing QObject's binding has already loaded its QtCore module.
        core = sys.modules.get(f"{binding}.QtCore")
        if core is None or not isinstance(obj, core.QObject):
            continue

        api = import_module(runtime)

        if binding.startswith("PyQt"):
            if api.isdeleted(obj):
                raise RuntimeError("The C++ QObject has been deleted")
            return int(api.unwrapinstance(api.cast(obj, core.QObject)))

        if not api.isValid(obj):
            raise RuntimeError("The C++ QObject wrapper is no longer valid")

        # Shiboken can expose multiple C++ instances for one Python object.
        addresses = {int(p) for p in api.getCppPointer(obj) if p}
        if len(addresses) != 1:
            raise ValueError("Cannot identify a unique native QObject address")
        return addresses.pop()

    raise TypeError("Expected a PyQt5, PyQt6, PySide2, or PySide6 QObject")





def qobject_from_address(address: int, *, extra_modules=()):
    """Wrap a live QObject* as its most specific available Qt type."""
    
    import qtpy
    
    if isinstance(address, bool) or not isinstance(address, int):
        raise TypeError("address must be an integer")
    if address < 0:
        raise ValueError("address must be nonnegative")
    if address == 0:
        return None

    binding = qtpy.API_NAME
    if binding == "PySide6":
        from shiboken6 import wrapInstance as wrap
    elif binding == "PyQt6":
        from PyQt6.sip import wrapinstance as wrap
    else:
        raise RuntimeError(f"Expected a Qt 6 binding, got {binding!r}")

    core = import_module(f"{binding}.QtCore")
    modules = (
        *extra_modules,
        import_module(f"{binding}.QtWidgets"),
        import_module(f"{binding}.QtGui"),
        core,
    )

    for name, ptr in builtins.internal.qobject_casts(address):
        for module in modules:
            cls = getattr(module, name, None)
            if isinstance(cls, type) and issubclass(cls, core.QObject):
                obj = wrap(ptr, cls)
                if not isinstance(obj, cls):
                    raise TypeError(
                        f"Requested {name}, but the binding returned "
                        f"an existing {type(obj).__name__} wrapper"
                    )
                return obj

    raise TypeError("No matching QObject wrapper found")