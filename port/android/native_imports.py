"""Keep mwcc's independently recovered extern views at native call sites.

An asm label changes only the C++ identifier: every import still names the
original ELF symbol and keeps its original parameter and return types. This is
limited to explicitly audited TUs and ROM symbols, never class member calls.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from check_decl_agreement import scrub, top_level_units  # noqa: E402
from rom_vtables import SPECIAL  # noqa: E402

TOKEN = re.compile(r'"C(?:\+\+)?"|[A-Za-z_]\w*|::|->|[^\s]')


def rom_symbols():
    result = set()
    for path in (ROOT / "config").rglob("symbols.txt"):
        for line in path.read_text().splitlines():
            if " kind:" in line:
                result.add(line.split()[0])
    return result


def isolate(text, symbols=None):
    code, _ = scrub(text)
    symbols = rom_symbols() if symbols is None else symbols
    explicit = []
    for start, head, term, linkage, _, _ in top_level_units(code, "C++"):
        # A prototype inside an extern "C" region need not repeat extern.
        # Adding it makes the same import handling apply to both spellings.
        if term != ";" or linkage != "C" or "(" not in head:
            continue
        prefix = head.split("(", 1)[0]
        words = re.findall(r"[A-Za-z_]\w*", prefix)
        if words and words[-1] in symbols and not re.search(r"\b(extern|typedef)\b|=", prefix):
            explicit.append(start + len(head) - len(head.lstrip()))
    for offset in reversed(explicit):
        text = text[:offset] + "extern " + text[offset:]
    code, _ = scrub(text)
    tokens = list(TOKEN.finditer(code))
    # Pair delimiters before editing. Comments, literals and directives are
    # blanked at the same offsets by the repository's existing lexer.
    pairs, stack = {}, []
    for i, token in enumerate(tokens):
        if token[0] in ("{", "(", "["):
            stack.append(i)
        elif token[0] in ("}", ")", "]"):
            if not stack:
                raise ValueError("unbalanced import source")
            opening = stack.pop()
            if (tokens[opening][0], token[0]) not in (("{", "}"), ("(", ")"), ("[", "]")):
                raise ValueError("mismatched import delimiters")
            pairs[opening] = i
            pairs[i] = opening
    if stack:
        raise ValueError("unclosed import source")

    declarations, definition_names = {}, set()
    for i, token in enumerate(tokens):
        # Protect each actual function definition's name. Its imports must not
        # rename the implementation that supplies the original ELF symbol.
        if token[0] == "{" and i and tokens[i - 1][0] == ")":
            opening = pairs[i - 1]
            if opening:
                definition_names.add(opening - 1)
        if token[0] != "extern":
            continue
        j = i + 1
        while j < len(tokens) and tokens[j][0] not in (";", "{", "}"):
            j += 1
        if j == len(tokens) or tokens[j][0] != ";":
            continue  # a linkage region or a definition, not an import
        prefix = []
        for k in range(i + 1, j):
            if tokens[k][0] in ("(", "["):
                break  # stop before parameter/array expressions
            prefix.append(k)
        identifiers = [k for k in prefix if re.fullmatch(r"[A-Za-z_]\w*", tokens[k][0])]
        if not identifiers or tokens[identifiers[-1]][0] not in symbols:
            continue
        if any(tokens[k][0] == "," for k in prefix):
            raise ValueError("ambiguous native import declaration")
        name_index = identifiers[-1]
        name = tokens[name_index][0]
        # Multiple data declarators need a deliberate extension; never let a
        # label accidentally bind a different member of a declaration list.
        if tokens[name_index + 1][0] not in ("(", "[", ";"):
            raise ValueError(f"unsupported native import declarator: {name}")
        k = name_index + 1
        while k < j:
            if tokens[k][0] in ("(", "["):
                k = pairs[k] + 1
                continue
            if tokens[k][0] == ",":
                raise ValueError("multiple native import declarators")
            k += 1
        alias = f"port_import_{len(declarations)}_{name}"
        declarations[name_index] = (name, alias, j)

    edits, scopes, transparent, namespaces = [], [{}], [], {}
    for i, token in enumerate(tokens):
        word = token[0]
        if word == "{":
            # extern "C" specifies linkage, not C++ lexical scope.
            linkage = i >= 2 and tokens[i - 2][0] == "extern" and tokens[i - 1][0] in ('"C"', '"C++"')
            transparent.append(linkage)
            if not linkage:
                namespace = i >= 2 and tokens[i - 2][0] == "namespace"
                scopes.append(namespaces.setdefault(tokens[i - 1][0], {}) if namespace else {})
        elif word == "}":
            if not transparent.pop():
                scopes.pop()
        elif i in declarations:
            name, alias, end = declarations[i]
            scopes[-1][name] = alias
            edits.append((token.start(), token.end(), alias))
            label = SPECIAL.get(name, "port_rom" + name) if name.startswith("_ZTV") else name
            edits.append((tokens[end].start(), tokens[end].start(), f' __asm__("{label}")'))
        elif word in symbols and i not in definition_names:
            # Qualified C++ names and member access keep compiler mangling.
            if i >= 2 and tokens[i - 1][0] == "::":
                alias = namespaces.get(tokens[i - 2][0], {}).get(word)
                if alias:
                    edits.append((token.start(), token.end(), alias))
                continue
            if i and tokens[i - 1][0] in (".", "->"):
                continue
            if i + 1 < len(tokens) and tokens[i + 1][0] == "::":
                continue
            alias = next((scope[word] for scope in reversed(scopes) if word in scope), None)
            if alias:
                edits.append((token.start(), token.end(), alias))
    for start, end, replacement in sorted(edits, reverse=True):
        text = text[:start] + replacement + text[end:]
    return text
