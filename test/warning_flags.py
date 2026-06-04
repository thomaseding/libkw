from pathlib import Path


def read_warning_flags(repo_dir: Path) -> list[str]:
    text = (repo_dir / "warning_flags.txt").read_text()
    return [line for line in text.splitlines() if line]
