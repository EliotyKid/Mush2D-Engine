"""LDtk's own JSON layout ("Compact" pretty print), so scripts that rewrite showcase.ldtk keep diffs small.

Small objects and arrays (estimated length <= 85) stay on one line, larger ones go one item per line (tab
indent); numeric arrays of more than 85 items become a grid of 35 values per line. Mirrors deepnight's
JsonPretty; dumps(json.loads(text)) == text for a file LDtk 1.5 saved (checked by the callers).
"""
import json

MAX = 85


def _is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def _est(v, top=True):
    if v is None or v is True or (isinstance(v, int) and not isinstance(v, bool)):
        return 4
    if v is False or isinstance(v, float):
        return 5
    if isinstance(v, str):
        return len(v) + 2
    if isinstance(v, dict):  # a field holding a list of strings or bools counts as long
        long = lambda x: isinstance(x, list) and x and not all(_is_num(y) or isinstance(y, dict) for y in x)
        return sum(99 if long(x) else _est(x, False) for x in v.values()) if top and len(v) <= 5 else len(v) * 10
    if not v:
        return 2
    if all(_is_num(x) for x in v) and len(v) < 50:
        return len(v)
    if len(v) > 5 or any(isinstance(x, list) for x in v):
        return 99
    return sum(_est(x, False) for x in v)


def _scalar(v):
    if isinstance(v, float) and v.is_integer():
        return str(int(v))
    return json.dumps(v, ensure_ascii=False)


def _dump(v, ind, key=None):
    """A one-line item keeps the indent of its line: its multiline children are indented from there."""
    tabs = "\t" * ind
    if isinstance(v, dict):
        if not v:
            return "{}"
        if key not in (None, "__header__") and _est(v) <= MAX:
            return "{ " + ", ".join(f"{json.dumps(k)}: {_dump(x, ind, k)}" for k, x in v.items()) + " }"
        items = [f"{json.dumps(k)}: {_dump(x, ind + 1, k)}" for k, x in v.items()]
        return "{\n" + ",\n".join(tabs + "\t" + i for i in items) + "\n" + tabs + "}"
    if isinstance(v, list):
        if not v:
            return "[]"
        nums = all(_is_num(x) for x in v)
        if nums and len(v) > MAX:
            rows = [",".join(_scalar(x) for x in v[i:i + 35]) for i in range(0, len(v), 35)]
            return "[\n" + ",\n".join(tabs + "\t" + r for r in rows) + "\n" + tabs + "]"
        if _est(v) <= MAX:
            parts = [_dump(x, ind, "") for x in v]
            bools = all(isinstance(x, bool) for x in v)
            if nums or (len(v) == 1 and not bools) or (bools and len(v) > 5):
                return "[" + ",".join(parts) + "]"
            return "[ " + ", ".join(parts) + " ]"
        return "[\n" + ",\n".join(tabs + "\t" + _dump(x, ind + 1, "") for x in v) + "\n" + tabs + "]"
    return _scalar(v)


def dumps(root, original_text):
    """root in LDtk's layout, with the newline style and trailing newline of original_text."""
    newline = "\r\n" if "\r\n" in original_text else "\n"
    text = _dump(root, 0)
    if original_text.endswith(("\n", "\r\n")):
        text += "\n"
    return text.replace("\n", newline)
