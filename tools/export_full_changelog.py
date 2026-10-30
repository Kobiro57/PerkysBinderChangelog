"""Render the complete reviewed German change history as a portable TXT file."""
from pathlib import Path
import re


def render(text):
    text = re.sub(r"!\[([^\]]*)\]\(([^)]+)\)", r"\1 (\2)", text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1 (\2)", text)
    lines = []
    for line in text.splitlines():
        heading = re.match(r"^(#{1,6})\s+(.+)$", line)
        if heading:
            title = heading[2].replace("**", "").replace("`", "")
            lines += [title, ("=" if len(heading[1]) <= 2 else "-") * len(title)]
        else:
            lines.append(line.replace("**", "").replace("`", ""))
    return "\n".join(lines).rstrip() + "\n"


def main():
    root = Path(__file__).resolve().parents[1]
    source = root / "docs/review/CHANGELOG-DE.md"
    target = root / "docs/CHANGELOG-GESAMT.txt"
    text = source.read_text(encoding="utf-8")
    required = {f"## {number}. " for number in range(1, 19)}
    if not all(marker in text for marker in required):
        raise ValueError("Complete change history sections 1–18 required")
    result = render(text)
    target.write_text(result, encoding="utf-8-sig", newline="\n")
    print(f"Full changelog: {len(result.splitlines())} lines, sections 1–18, {target.name}")


if __name__ == "__main__":
    main()
