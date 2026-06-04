import argparse
import re
import sys
from pathlib import Path


def canonicalize_asm(content: str, *, canonicalize_slots: bool) -> str:
    """Normalize x86-64 Intel-syntax assembly so that semantically identical
    code produced by different compilations compares equal as text.

    WARNING: The output is NOT valid x86-64 assembly and cannot be reassembled.
    It is intended solely for diffing and equality comparison.

    Two sources of cosmetic divergence are erased:

    1. GCC label numbers (.L<prefix><N>)
       GCC assigns sequential integers to all internal labels as it lowers each
       translation unit, including bare jump labels (.L<N>), function-boundary
       markers (.LFB<N>, .LFE<N>, .LFSB<N>), and exception-handling LSDA
       labels (.LLSDA<N>, .LLSDAC<N>, .LLSDACSB<N>, .LLSDACSE<N>).  Two TUs
       that differ only in which preprocessor branch is active — or differ in
       how many template instantiations are emitted — assign different integers
       to the same logical label.  We renumber each family independently by
       order of first appearance (.LFB0, .LFB1, ...; .LLSDA0, .LLSDA1, ...)
       so the numbering reflects only structural position in the output.

    2. RSP-relative stack-slot offsets (N[rsp])
       N is a byte offset from rsp, so sizeof affects it: a 4-byte int at
       offset 8 occupies bytes 8-11, and the next variable starts at 12.
       When a function passes by-value arguments to a callee, the compiler
       must copy-/move-construct them into fresh stack slots before the call.
       The byte offset assigned to each slot depends on the order the
       compiler's stack allocator first encounters each temporary — which can
       differ between a direct positional call and one routed through the KW
       template machinery, even after full inlining.  We replace each distinct
       N[rsp] token with a SLOT<k>[rsp] placeholder in order of first
       appearance (-> SLOT0[rsp], SLOT1[rsp], ...).  The SLOT prefix is
       intentionally invalid x86-64 syntax: it makes the canonical tokens
       visually distinct from real byte offsets and reinforces the WARNING
       above that the output cannot be reassembled.  The index k is a slot
       ordinal, not a byte offset — sizeof and alignment are not reflected
       in it.

    Both passes use the same strategy: scan left-to-right, assign a fresh
    canonical index the first time each distinct token is seen, and reuse that
    index on every subsequent occurrence.  The result is idempotent.
    """
    label_map: dict[str, str] = {}
    label_counters: dict[str, int] = {}

    def replace_label(m: re.Match) -> str:
        key = m.group(0)
        if key not in label_map:
            prefix = m.group(1)
            n = label_counters.get(prefix, 0)
            label_map[key] = f".L{prefix}{n}"
            label_counters[prefix] = n + 1
        return label_map[key]

    content = re.sub(r"\.L([A-Za-z]*)(\d+)", replace_label, content)

    slot_map: dict[str, str] = {}
    slot_counter = 0

    def replace_slot(m: re.Match) -> str:
        nonlocal slot_counter
        key = m.group(0)
        if key not in slot_map:
            slot_map[key] = f"SLOT{slot_counter}[rsp]"
            slot_counter += 1
        return slot_map[key]

    if canonicalize_slots:
        content = re.sub(r"\d+\[rsp\]", replace_slot, content)

    return content


def asm_files_match(path_a: Path, path_b: Path, *, canonicalize_slots: bool) -> bool:
    return canonicalize_asm(
        path_a.read_text(), canonicalize_slots=canonicalize_slots
    ) == canonicalize_asm(path_b.read_text(), canonicalize_slots=canonicalize_slots)


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Compare two x86-64 assembly files after canonicalization."
    )
    parser.add_argument("file_a", type=Path, metavar="file_a.s")
    parser.add_argument("file_b", type=Path, metavar="file_b.s")
    parser.add_argument(
        "--no-canonicalize-slots",
        action="store_true",
        help="Skip RSP slot canonicalization",
    )
    return parser


def parse_args() -> argparse.Namespace:
    return make_parser().parse_args()


def main() -> None:
    args = parse_args()
    canonicalize_slots = not args.no_canonicalize_slots
    a = args.file_a
    b = args.file_b
    ca = canonicalize_asm(a.read_text(), canonicalize_slots=canonicalize_slots)
    cb = canonicalize_asm(b.read_text(), canonicalize_slots=canonicalize_slots)

    if ca == cb:
        print("MATCH")
    else:
        import difflib

        sys.stdout.writelines(
            difflib.unified_diff(
                ca.splitlines(keepends=True),
                cb.splitlines(keepends=True),
                fromfile=str(a),
                tofile=str(b),
            )
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
