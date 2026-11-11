#!/usr/bin/env python3
"""Compile and execute preserved import views; never require Nintendo data."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import native_imports
import rom_vtables


class NativeImportsTest(unittest.TestCase):
    def test_call_types_scopes_and_link_names(self):
        source = r'''
extern "C" int read_word(const int*);
extern "C" int values[];
extern "C" {
int first(const int *p) {
    extern int read_word(const void*);
    extern char values[];
    // read_word(values) is a comment, not a call.
    const char *label = "read_word(values)";
    return read_word(p) + values[0] + (label[0] == 'r');
}
}
namespace alternate { extern "C" int read_word(const void *); }
extern "C" {
int read_word(const int *p) { return *p; }
int second(const int *p) {
    return alternate::read_word(p) + values[1];
}
}
extern "C" int values[] = {2, 5};
int main() {
    int x = 7;
    return first(&x) == 10 && second(&x) == 12 ? 0 : 1;
}
'''
        # The definition of a data array in this fixture is deliberately after
        # all recovered views, as in ROM-clean mounts (a separate TU in game).
        source = source.replace('extern "C" int values[] = {2, 5};', '')
        transformed = native_imports.isolate(source, {"read_word", "values"})
        self.assertIn('"read_word(values)"', transformed)
        self.assertIn('// read_word(values) is a comment', transformed)
        self.assertIn('int read_word(const int *p) { return *p; }', transformed)
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "views.cpp").write_text(transformed)
            (root / "data.cpp").write_text('extern "C" { int values[] = {2, 5}; }')
            binary = root / "check"
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            str(root / "views.cpp"), str(root / "data.cpp"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            imports = subprocess.check_output(["nm", "-u", str(binary)], text=True)
            self.assertNotIn("port_import_", imports)

    def test_ambiguous_data_declarations_fail_closed(self):
        with self.assertRaises(ValueError):
            native_imports.isolate("extern int one, two;", {"one", "two"})
        with self.assertRaises(ValueError):
            native_imports.isolate("extern int one[], two[];", {"one", "two"})

    def test_unbalanced_source_fails_closed(self):
        with self.assertRaises(ValueError):
            native_imports.isolate("void f() {", set())

    def test_raw_ds_table_cannot_bind_native_compiler_prefix(self):
        source = '''
class dScene_c { public: virtual int tick(); };
int dScene_c::tick() { return 3; }
extern "C" { extern void *_ZTV8dScene_c[];
void **raw_ds_table() { return _ZTV8dScene_c; } }
'''
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            header = root / "tables.h"
            rom_vtables.emit(header)
            for mode, text in (("tokens", source), ("asm_labels", native_imports.isolate(source, {"_ZTV8dScene_c"}))):
                cpp, obj = root / (mode + ".cpp"), root / (mode + ".o")
                cpp.write_text(text)
                subprocess.run(["g++", "-std=c++17", "-include", str(header),
                                "-c", str(cpp), "-o", str(obj)], check=True)
                definitions = subprocess.check_output(["nm", "--defined-only", str(obj)], text=True)
                imports = subprocess.check_output(["nm", "-u", str(obj)], text=True)
                self.assertIn("_ZTV8dScene_c", definitions)
                self.assertIn("port_vt_scene_rom", imports)
                self.assertNotIn("port_vt_scene_rom", definitions)


if __name__ == "__main__":
    unittest.main()
