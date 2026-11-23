"""Minimal KiCad s-expression parser / writer (no external deps)."""
import re

_TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))')


class Sym(str):
    """Bare (unquoted) atom."""


def parse(text):
    pos = 0
    stack = [[]]
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if not m:
            break
        pos = m.end()
        if m.group(1):
            stack.append([])
        elif m.group(2):
            lst = stack.pop()
            stack[-1].append(lst)
        elif m.group(3) is not None:
            stack[-1].append(m.group(3).replace('\\"', '"').replace('\\\\', '\\'))
        elif m.group(4) is not None:
            stack[-1].append(Sym(m.group(4)))
    return stack[0][0] if stack[0] else None


def _q(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def dump(node, indent=0, step=2):
    """Pretty print: lists whose children are all atoms stay on one line."""
    pad = ' ' * indent
    if isinstance(node, list):
        if all(not isinstance(c, list) for c in node):
            return pad + '(' + ' '.join(_atom(c) for c in node) + ')'
        head = []
        i = 0
        while i < len(node) and not isinstance(node[i], list):
            head.append(_atom(node[i]))
            i += 1
        out = [pad + '(' + ' '.join(head)]
        for c in node[i:]:
            if isinstance(c, list):
                out.append(dump(c, indent + step, step))
            else:
                out.append(pad + ' ' * step + _atom(c))
        out.append(pad + ')')
        return '\n'.join(out)
    return pad + _atom(node)


def _atom(a):
    if isinstance(a, Sym):
        return str(a)
    if isinstance(a, bool):
        return 'yes' if a else 'no'
    if isinstance(a, float):
        s = f'{a:.6f}'.rstrip('0').rstrip('.')
        return s if s not in ('-0', '') else '0'
    if isinstance(a, int):
        return str(a)
    return _q(str(a))


def find(node, key):
    """First child list whose head == key."""
    for c in node:
        if isinstance(c, list) and c and c[0] == key:
            return c
    return None


def find_all(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]
