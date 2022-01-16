class Action:
    name = property(lambda self: self.__name)
    implicit = property(lambda self: False)
    rule = property(lambda self: None)
    deps = property(lambda self: tuple())
    parse_deps = property(lambda self: False)

    def __init__(self, name):
        self.__name = name

    def output_of(self, path):
        return None


class Compile(Action):
    rule = property(lambda self: f'compile_{self.name}')
    deps = property(lambda self: ("${module_dir}/.parse_dep.phony",))
    parse_deps = property(lambda self: True)

    def __init__(self, name, suffix = ".o"):
        super().__init__(name)
        self.__suffix = suffix

    def output_of(self, path):
        return str(path) + self.__suffix


class Parse(Action):
    rule = property(lambda self: f'parse_{self.name}')

    def output_of(self, path):
        suffix = "." + self.name
        if path.suffix == suffix:
            return path.with_suffix('')
        return path


class Link(Action): pass


class Header(Action):
    implicit = property(lambda self: True)


class Source:
    Actions = {
        '.c': Compile('c'),
        '.cpp': Compile('cxx'),
        '.s': Compile('asm'),
        '.cog': Parse('cog'),
        '.o': Link('o'),
        '.h': Header('h'),
        '.inc': Header('inc'),
    }

    def __init__(self, root, path, output=None, deps=tuple()):
        from pathlib import Path
        self.__root = Path(root)
        self.__path = Path(path)
        self.__output = output
        self.__deps = tuple(deps)

    def __str__(self):
        return self.input

    def add_deps(self, deps):
        self.__deps += tuple(deps)

    @property
    def action(self):
        suffix = self.__path.suffix
        return self.Actions.get(suffix)

    @property
    def output(self):
        if not self.action:
            return None

        path = self.__output
        if path is None:
            path = self.action.output_of(self.__path)

        return path and Source("${module_dir}", path)

    deps = property(lambda self: self.__deps)
    name = property(lambda self: str(self.__path))
    input = property(lambda self: str(self.__root / self.__path))
