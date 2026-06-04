import argparse
import os
import subprocess
import sys

from dataclasses import dataclass
from pathlib import Path

from determine_matching_asm import canonicalize_asm
from warning_flags import read_warning_flags

ATTR_MODES = [
    "TEST_NOIPA",
    "TEST_NOINLINE",
    "TEST_ALWAYS_INLINE",
    "TEST_EMPTY_ATTR",
]

CALL_MODES = [
    "TEST_POSITIONAL_CALL",
    "TEST_KW_CALL_IN_ORDER",
    "TEST_KW_CALL_OUT_OF_ORDER",
]

# -O0 and -Og mismatch, but that's expected and fine.
OPTIMIZE_LEVELS = [
    "-O1",
    "-O2",
    "-O3",
    "-Os",
    "-Ofast",
    "-Oz",
]

TARGETS = [
    "x86-64",
]


@dataclass(frozen=True)
class Combo:
    attr_mode: str
    call_mode: str
    optimize_level: str
    target: str


def get_combinations() -> list[Combo]:
    combos: list[Combo] = []
    for attr_mode in ATTR_MODES:
        for call_mode in CALL_MODES:
            for optimize_level in OPTIMIZE_LEVELS:
                for target in TARGETS:
                    combos.append(
                        Combo(
                            attr_mode=attr_mode,
                            call_mode=call_mode,
                            optimize_level=optimize_level,
                            target=target,
                        )
                    )
    return combos


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--keep-asm",
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Write canonicalized assembly files to disk.",
    )
    parser.add_argument(
        "--run-test",
        default=True,
        action=argparse.BooleanOptionalAction,
        help="Pairwise-compare generated assembly files for matches.",
    )
    parser.add_argument(
        "--canonicalize-slots",
        default=True,
        action=argparse.BooleanOptionalAction,
        help="Replace N[rsp] byte offsets with SLOT<k>[rsp] ordinals before comparing.",
    )
    return parser


@dataclass(frozen=True)
class CliArgs:
    keep_asm: bool
    run_test: bool
    canonicalize_slots: bool
    src_file: Path
    include_dir: Path
    output_dir: Path
    warning_flags: list[str]


def parse_args() -> argparse.Namespace:
    args = create_parser().parse_args()
    script_dir = Path(__file__).parent
    repo_dir = script_dir.parent
    return CliArgs(
        keep_asm=args.keep_asm,
        run_test=args.run_test,
        canonicalize_slots=args.canonicalize_slots,
        src_file=script_dir / "modal.cpp",
        include_dir=repo_dir / "src",
        output_dir=repo_dir / "build" / "modal_output",
        warning_flags=read_warning_flags(repo_dir),
    )


def _target_flags_and_ext(target: str) -> tuple[list[str], str]:
    if target == "x86-64":
        return ["-S", "-masm=intel"], ".s"
    raise ValueError(f"Unknown target: {target}")


def _combo_output_file(combo: Combo, output_dir: Path) -> Path:
    _, ext = _target_flags_and_ext(combo.target)
    opt_safe = combo.optimize_level.lstrip("-")
    name = f"modal_{combo.target}_{combo.attr_mode}_{combo.call_mode}_{opt_safe}{ext}"
    return output_dir / name


def equals_ignoring_call_mode(a: Combo, b: Combo) -> bool:
    return (
        a.attr_mode == b.attr_mode
        and a.optimize_level == b.optimize_level
        and a.target == b.target
    )


@dataclass(frozen=True)
class GenerateAsmResult:
    combos: list[Combo]
    successful: set[Combo]
    asm_content: dict[Combo, str]
    failures: int


def generate_asm(args: CliArgs) -> GenerateAsmResult:
    args.output_dir.mkdir(parents=True, exist_ok=True)

    combos = get_combinations()
    successful: set[Combo] = set()
    asm_content: dict[Combo, str] = {}
    failures = 0
    for combo in combos:
        target_flags, _ = _target_flags_and_ext(combo.target)
        output_file = _combo_output_file(combo, args.output_dir)

        cmd = [
            os.environ.get("GXX", "g++"),
            "-std=c++26",
            "-freflection",
            *args.warning_flags,
            combo.optimize_level,
            *target_flags,
            f"-D{combo.attr_mode}",
            f"-D{combo.call_mode}",
            f"-I{args.include_dir}",
            str(args.src_file),
            "-o",
            str(output_file),
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"FAIL: {output_file.name}")
            print(result.stderr)
            failures += 1
        else:
            content = canonicalize_asm(
                output_file.read_text(), canonicalize_slots=args.canonicalize_slots
            )
            if args.keep_asm:
                output_file.write_text(content)
            print(f"OK:   {output_file.name}")
            successful.add(combo)
            asm_content[combo] = content

    return GenerateAsmResult(
        combos=combos,
        successful=successful,
        asm_content=asm_content,
        failures=failures,
    )


@dataclass(frozen=True)
class RunTestResult:
    match_failures: int
    match_total: int


def run_test(asm: GenerateAsmResult, output_dir: Path) -> RunTestResult:
    print()
    match_failures = 0
    match_total = 0
    for i, combo1 in enumerate(asm.combos):
        for combo2 in asm.combos[i + 1 :]:
            if not equals_ignoring_call_mode(combo1, combo2):
                continue
            match_total += 1
            matched = asm.asm_content[combo1] == asm.asm_content[combo2]
            label = "MATCH:   " if matched else "MISMATCH:"
            if not matched:
                match_failures += 1
            name_a = _combo_output_file(combo1, output_dir).name
            name_b = _combo_output_file(combo2, output_dir).name
            print(f"{label} {name_a} <-> {name_b}")
    if match_total > 0:
        print(f"\n{match_total - match_failures}/{match_total} pairs match")
    return RunTestResult(
        match_failures=match_failures,
        match_total=match_total,
    )


def main() -> None:
    failed = False
    args = parse_args()

    asm = generate_asm(args)
    print(
        f"\n{len(asm.combos) - asm.failures}/{len(asm.combos)} compiled -> {args.output_dir}"
    )
    if asm.failures > 0:
        print(f"FAIL: {asm.failures} combo(s) failed to build, see errors above")
        sys.exit(1)

    if args.run_test:
        test = run_test(asm, args.output_dir)
        failed = failed or test.match_failures > 0

    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
