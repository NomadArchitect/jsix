def _indent(x):
    from textwrap import indent
    return indent(str(x), '    ')

class Description(str): pass
class Import(str): pass

class Options(set):
    def __str__(self):
        if not self: return ""
        return "[{}]".format(" ".join(self))

class UID(int):
    def __str__(self):
        return f"{self:016x}"

from .object import Object
from .interface import Interface, Expose
from .function import Function, Method, Param

from .type import Type
from .primitive import get_primitive
from .objref import ObjectRef