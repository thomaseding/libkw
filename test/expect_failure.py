import argparse
import difflib
import os
import subprocess
import sys

from dataclasses import dataclass
from pathlib import Path

from warning_flags import read_warning_flags


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--auto-update-oracles",
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Overwrite .oracle files with the actual build output instead of comparing against them.",
    )
    return parser


@dataclass(frozen=True)
class CliArgs:
    auto_update_oracles: bool
    repo_dir: Path
    src_dir: Path
    include_dir: Path
    warning_flags: list[str]


def parse_args() -> CliArgs:
    args = create_parser().parse_args()
    script_dir = Path(__file__).parent
    repo_dir = script_dir.parent
    return CliArgs(
        auto_update_oracles=args.auto_update_oracles,
        repo_dir=repo_dir,
        src_dir=script_dir / "expect_failure",
        include_dir=repo_dir / "src",
        warning_flags=read_warning_flags(repo_dir),
    )


@dataclass(frozen=True)
class Scenario:
    src_file: Path
    oracle_file: Path


def discover_scenarios(src_dir: Path) -> list[Scenario]:
    return [
        Scenario(src_file=src_file, oracle_file=src_file.with_suffix(".oracle"))
        for src_file in sorted(src_dir.glob("*.cpp"))
    ]


def compile_scenario(
    scenario: Scenario, repo_dir: Path, include_dir: Path, warning_flags: list[str]
) -> subprocess.CompletedProcess:
    cmd = [
        os.environ.get("GXX", "g++"),
        "-std=c++26",
        "-freflection",
        *warning_flags,
        f"-I{include_dir.relative_to(repo_dir)}",
        "-c",
        str(scenario.src_file.relative_to(repo_dir)),
        "-o",
        os.devnull,
    ]
    return subprocess.run(cmd, capture_output=True, text=True, cwd=repo_dir)


STATIC_ASSERT_MARKER = "error: static assertion failed"


@dataclass(frozen=True)
class RunResult:
    scenarios: list[Scenario]
    failures: int


def run_scenarios(scenarios: list[Scenario], args: CliArgs) -> RunResult:
    failures = 0
    for scenario in scenarios:
        result = compile_scenario(
            scenario, args.repo_dir, args.include_dir, args.warning_flags
        )
        actual = result.stdout + result.stderr

        if result.returncode == 0:
            print(
                f"FAIL: {scenario.src_file.name} (compiled successfully, but was expected to fail)"
            )
            failures += 1
            continue

        assertion_count = actual.count(STATIC_ASSERT_MARKER)
        if assertion_count > 1:
            print(
                f"FAIL: {scenario.src_file.name} ({assertion_count} occurrences of {STATIC_ASSERT_MARKER!r}, expected at most 1; "
                f"likely fix: add more if constexpr{{}} else{{}} branching in libkw.hpp to short-circuit before a subsequent static_assert also fires)"
            )
            failures += 1
            continue

        if args.auto_update_oracles:
            scenario.oracle_file.write_text(actual)
            print(f"UPDATED: {scenario.oracle_file.name}")
            continue

        if not scenario.oracle_file.exists():
            print(
                f"FAIL: {scenario.src_file.name} (missing {scenario.oracle_file.name}, run with --auto-update-oracles)"
            )
            failures += 1
            continue

        expected = scenario.oracle_file.read_text()
        if actual != expected:
            print(
                f"FAIL: {scenario.src_file.name} (output mismatch with {scenario.oracle_file.name})"
            )
            sys.stdout.writelines(
                difflib.unified_diff(
                    expected.splitlines(keepends=True),
                    actual.splitlines(keepends=True),
                    fromfile=str(scenario.oracle_file),
                    tofile="actual",
                )
            )
            failures += 1
            continue

        print(f"OK:   {scenario.src_file.name}")

    return RunResult(scenarios=scenarios, failures=failures)


def main() -> None:
    args = parse_args()
    scenarios = discover_scenarios(args.src_dir)

    result = run_scenarios(scenarios, args)
    print(f"\n{len(result.scenarios) - result.failures}/{len(result.scenarios)} passed")

    if result.failures > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
