"""Represent 2D shapes and organize them into named groups."""

import numpy as np
import builtins

def __call_thermavip_fun(name, args = tuple(), kwargs={}):
    """Dispatch name and the supplied arguments through
    builtins.internal.call_internal_func.
    """
    args = (name,) + args
    return builtins.internal.call_internal_func(*args, **kwargs)

def call_thermavip_fun(name, args=tuple(), kwargs={}):
    """Call a function registered with vipRegisterFunction and return its result. Pass
    positional arguments as a tuple and keywords as a dictionary.
    """
    return __call_thermavip_fun(name, args, kwargs)


class VipShape:
    """Represent a 2D shape by its vertices, group name, and optional attributes.

    Geometry is inferred from the coordinates. A single vertex is a point.
    With two or more vertices, matching endpoints indicate a polygon and
    different endpoints indicate a polyline."""

    def __init__(self, points = list(), attributes ={}, group = str() ):
        """Build a shape from an (x, y) pair or a sequence of coordinate pairs.

        Omitted, empty, or None points create a null shape. Attributes are
        shallow-copied with dict(). The group defaults to the empty string."""
        self.points = points
        self.attributes = attributes
        self.group = group

    def __str__(self):
        """Return the group, points, and attributes on separate lines."""
        return f"group: '{self.group}'\npoints: {self.points}\nattributes: {self.attributes}"

    def __repr__(self):
        """Return the same multiline description as str(self)."""
        return self.__str__()

    @staticmethod
    def ellipse(left, top, width, height, attributes = {}):
        """Create an elliptic shape from a bounding rectangle.
        """
        return call_thermavip_fun("create_ellipse",(left, top, width, height,attributes))

    @property
    def group(self):
        """Return the shape's group name as a string."""
        return self.__group

    @group.setter
    def group(self, gr):
        """Store gr as a string, using the empty string for None."""
        if gr == None:
            self.__group = str()
        else:
            self.__group = str(gr)

    @property
    def points(self):
        """Return the stored NumPy array of (x, y) rows.

        A null shape has a zero-dimensional array. Editing the returned
        array in place changes the shape."""
        return self.__points

    @points.setter
    def points(self, p):
        """Store one coordinate pair as a (1, 2) array or multiple pairs as (N, 2).

        None and empty sequences select the zero-dimensional null representation.
        For a nonempty array with unsupported dimensions, the validation
        branches reference an undefined ArgumentError and raise NameError."""
        if p is None:
            p = np.empty(())
        else:
            if len(p) == 0:
                p = np.empty(())
            else:
                p = np.array(p)
        if len(p.shape) == 1:
            if p.shape[0] != 2:
                raise ArgumentError("invalid points")
            p.shape = (1,2)
        elif len(p.shape) == 2:
            if p.shape[1] != 2:
                raise ArgumentError("invalid points")
        elif len(p.shape) != 0:
            raise ArgumentError("invalid points")
        self.__points = p

    @property
    def attributes(self):
        """Return the live dictionary of shape attributes."""
        return self.__attributes

    @attributes.setter
    def attributes(self, a):
        """Shallow-copy a mapping or iterable of key-value pairs into the shape."""
        self.__attributes = dict(a)

    def is_null(self):
        """Return True if the points array has zero dimensions."""
        return len(self.points.shape) == 0

    def is_point(self):
        """Return True if the points array contains exactly one coordinate pair."""
        return len(self.points.shape) == 2 and self.points.shape[0] == 1

    def is_polyline(self):
        """Return True if there are at least two vertices and the endpoints differ."""
        return len(self.points.shape) == 2 and self.points.shape[0] > 1 and not bool((self.points[0] == self.points[-1]).all())

    def is_polygon(self):
        """Return True if there are at least two vertices and the endpoints match.

        The test uses exact coordinate equality and permits repeated vertices."""
        return len(self.points.shape) == 2 and self.points.shape[0] > 1 and bool((self.points[0] == self.points[-1] ).all())

    def fill_pixels(self):
        """Returns the pixels actually filled by this shape.
        """
        return call_thermavip_fun("fill_pixels", (self,))


class VipSceneModel:
    """Collect VipShape objects by group and hold scene-level attributes.

    Shapes are stored by reference. The group key is chosen on insertion.
    Editing a shape's group later leaves it under its original key."""

    def __init__(self, shapes = (), attributes ={} ):
        """Create a scene from an iterable of shapes or a plain dict of group lists.

        An iterable is indexed by each shape's group. A plain dict is retained
        by reference, as is the scene's attributes argument. Omitting
        attributes uses a dictionary shared by default-constructed scenes."""
        self.__shapes = {}
        self.shapes = shapes
        self.attributes = attributes

    def add_shape(self, sh : VipShape, group = None):
        """Append sh to its group, creating the group on first use.

        If group is supplied and is not None, update sh.group before insertion.
        Adding the same object repeatedly adds another reference each time."""

        if group is not None:
            sh.group = group

        try:
            self.__shapes[sh.group].append(sh)
        except:
            self.__shapes[sh.group] = [sh]   
    
    def add_shapes(self, shs, group = None):
        """Add a VipShape instance or an iterable of shapes in iteration order.

        The single-shape shortcut accepts the exact VipShape type. With a
        group override, the iterable is traversed once to set group names
        and again to add shapes. A one-shot iterator is exhausted by the
        first pass, so use a list or tuple when overriding the group."""
        if type(shs) == VipShape:
            return self.add_shape(shs,group)
        
        if group is not None:
            for s in shs: s.group = group
        for s in shs:
            self.add_shape(s)


    def shapes_for_group(self, group):
        """Return the stored list for group, or a new empty list if absent.

        Editing an existing group's list changes the scene."""
        try :
            return self.__shapes[group] 
        except:
            return[]

    @property
    def groups(self):
        """Return a new list of group names in dictionary insertion order."""
        return list(self.__shapes.keys())

    @property
    def shapes(self):
        """Return a new flat list of shape references, ordered by group then by position within each group."""
        ret =[]
        for key,value in self.__shapes.items():
            ret = ret + value
        return ret

    @shapes.setter 
    def shapes(self, shs):
        """Replace the collection with shs, a plain dict or an iterable of shapes.

        A plain dict is stored directly, without checking its keys against
        the shapes' group names. An iterable rebuilds the index from each
        shape's current group."""

        if type(shs) == dict:
            self.__shapes = shs
        else: 
            self.__shapes = {}
            for sh in shs:
                self.add_shape(sh)


