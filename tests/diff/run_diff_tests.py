#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, List, Sequence

SKIP_RETURN_CODE = 125


@dataclass
class CompilerRun:
    success: bool
    return_code: int
    semantics: str
    output: str


@dataclass
class CaseResult:
    name: str
    shader: str
    glsl2llvm: CompilerRun
    glslang: CompilerRun
    passed: bool
    checks: List[str]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run glsl2llvm vs glslangValidator differential tests")
    parser.add_argument("--glsl2llvm", required=True, help="Path to glsl2llvm executable")
    parser.add_argument("--glslang", default="glslangValidator", help="Path or command name for glslangValidator")
    parser.add_argument("--cases", required=True, help="Path to JSON test manifest")
    parser.add_argument("--work-dir", required=True, help="Working directory for temporary IR files")
    parser.add_argument("--report", required=True, help="Output markdown report path")
    return parser.parse_args()


def resolve_executable(name_or_path: str) -> str | None:
    if not name_or_path:
        return None
    path = Path(name_or_path)
    if path.is_absolute() or os.sep in name_or_path:
        return str(path) if path.exists() else None
    return shutil.which(name_or_path)


def normalize_output(output: str) -> str:
    return output.strip().replace("\r\n", "\n")


def classify_semantics(success: bool, output: str) -> str:
    if success:
        return "ok"

    text = output.lower()

    if (
        "undefined variable" in text
        or "undeclared identifier" in text
        or "undefined function" in text
    ):
        return "undefined_symbol"

    if "duplicate" in text or "redefinition" in text:
        return "duplicate_definition"

    if (
        "type mismatch" in text
        or "cannot convert" in text
        or "boolean expression expected" in text
    ):
        return "type_mismatch"

    if "unsupported" in text or "invalid member access" in text:
        return "unsupported_feature"

    if (
        "parser error" in text
        or "lexer error" in text
        or "syntax error" in text
        or "expected" in text
        or "unexpected" in text
    ):
        return "syntax_error"

    return "other_failure"


def run_command(command: Sequence[str]) -> tuple[int, str]:
    completed = subprocess.run(command, capture_output=True, text=True)
    merged = (completed.stdout or "") + (completed.stderr or "")
    return completed.returncode, normalize_output(merged)


def contains_all(output: str, required: Iterable[str]) -> tuple[bool, str]:
    for token in required:
        if token not in output:
            return False, token
    return True, ""


def short_output(text: str, max_lines: int = 2) -> str:
    if not text:
        return ""
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if not lines:
        return ""
    if len(lines) <= max_lines:
        return " | ".join(lines)
    return " | ".join(lines[:max_lines]) + " | ..."


def run_compiler(exe: str, shader: Path, out_path: Path | None) -> CompilerRun:
    if out_path is None:
        command = [exe, "-S", "comp", str(shader)]
    else:
        command = [exe, str(shader), "-S", "-o", str(out_path)]

    return_code, output = run_command(command)
    success = return_code == 0
    semantics = classify_semantics(success, output)
    return CompilerRun(success=success, return_code=return_code, semantics=semantics, output=output)


def render_report(results: list[CaseResult], report_path: Path, glsl2llvm_path: str, glslang_path: str) -> None:
    total = len(results)
    passed = sum(1 for item in results if item.passed)
    failed = total - passed

    mismatch_cases = [
        item
        for item in results
        if item.glsl2llvm.success != item.glslang.success
        or item.glsl2llvm.semantics != item.glslang.semantics
    ]

    unsupported_features = []
    for item in results:
        if item.glsl2llvm.semantics == "unsupported_feature":
            unsupported_features.append(f"- `{item.name}`: {short_output(item.glsl2llvm.output, max_lines=1)}")

    lines: list[str] = []
    lines.append("# Differential Compatibility Report")
    lines.append("")
    lines.append(f"Generated: {datetime.now(timezone.utc).isoformat()}")
    lines.append(f"glsl2llvm: `{glsl2llvm_path}`")
    lines.append(f"glslangValidator: `{glslang_path}`")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- Total cases: {total}")
    lines.append(f"- Passed: {passed}")
    lines.append(f"- Failed: {failed}")
    lines.append(f"- Semantic/status differences (actual): {len(mismatch_cases)}")
    lines.append("")
    lines.append("## Case Matrix")
    lines.append("")
    lines.append("| Case | glsl2llvm | glslang | Semantics (g2 / gs) | Status |")
    lines.append("| --- | --- | --- | --- | --- |")

    for item in results:
        g2 = "ok" if item.glsl2llvm.success else "fail"
        gs = "ok" if item.glslang.success else "fail"
        status = "PASS" if item.passed else "FAIL"
        lines.append(
            f"| `{item.name}` | {g2} (rc={item.glsl2llvm.return_code}) | {gs} (rc={item.glslang.return_code}) | "
            f"`{item.glsl2llvm.semantics}` / `{item.glslang.semantics}` | {status} |"
        )

    lines.append("")
    lines.append("## Unsupported Features Observed In glsl2llvm")
    lines.append("")
    if unsupported_features:
        lines.extend(unsupported_features)
    else:
        lines.append("- None in this run.")

    lines.append("")
    lines.append("## Semantic Differences")
    lines.append("")
    if mismatch_cases:
        for item in mismatch_cases:
            lines.append(
                f"- `{item.name}`: glsl2llvm=`{item.glsl2llvm.semantics}` vs "
                f"glslang=`{item.glslang.semantics}`"
            )
    else:
        lines.append("- None in this run.")

    failures = [item for item in results if not item.passed]
    lines.append("")
    lines.append("## Failing Checks")
    lines.append("")
    if not failures:
        lines.append("- None.")
    else:
        for item in failures:
            lines.append(f"- `{item.name}`")
            for check in item.checks:
                lines.append(f"  - {check}")

    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()

    glsl2llvm_exe = resolve_executable(args.glsl2llvm)
    if glsl2llvm_exe is None:
        print(f"error: glsl2llvm executable not found: {args.glsl2llvm}", file=sys.stderr)
        return 1

    glslang_exe = resolve_executable(args.glslang)
    if glslang_exe is None:
        print(
            f"skip: glslangValidator not found (looked for: {args.glslang})",
            file=sys.stderr,
        )
        return SKIP_RETURN_CODE

    cases_path = Path(args.cases).resolve()
    data = json.loads(cases_path.read_text(encoding="utf-8"))
    cases = data.get("cases", [])
    min_cases = int(data.get("min_cases", 10))

    if len(cases) < min_cases:
        print(
            f"error: expected at least {min_cases} differential shader cases, got {len(cases)}",
            file=sys.stderr,
        )
        return 1

    work_dir = Path(args.work_dir).resolve() / "diff-work"
    work_dir.mkdir(parents=True, exist_ok=True)

    results: list[CaseResult] = []
    has_failures = False

    for index, case in enumerate(cases):
        case_name = case["name"]
        shader_path = (cases_path.parent / case["shader"]).resolve()

        if not shader_path.exists():
            print(f"error: shader file does not exist: {shader_path}", file=sys.stderr)
            return 1

        ir_out = work_dir / f"case_{index:02d}.ll"

        g2 = run_compiler(glsl2llvm_exe, shader_path, ir_out)
        gs = run_compiler(glslang_exe, shader_path, None)

        checks: list[str] = []

        expected_g2_success = bool(case["expect_glsl2llvm_success"])
        expected_gs_success = bool(case["expect_glslang_success"])

        if g2.success != expected_g2_success:
            checks.append(
                f"glsl2llvm success mismatch: expected {expected_g2_success}, got {g2.success}"
            )

        if gs.success != expected_gs_success:
            checks.append(
                f"glslang success mismatch: expected {expected_gs_success}, got {gs.success}"
            )

        expected_g2_sem = case.get("expect_glsl2llvm_semantics")
        expected_gs_sem = case.get("expect_glslang_semantics")
        if expected_g2_sem and g2.semantics != expected_g2_sem:
            checks.append(
                f"glsl2llvm semantics mismatch: expected {expected_g2_sem}, got {g2.semantics}"
            )
        if expected_gs_sem and gs.semantics != expected_gs_sem:
            checks.append(
                f"glslang semantics mismatch: expected {expected_gs_sem}, got {gs.semantics}"
            )

        required_g2 = case.get("expect_glsl2llvm_error_substr", [])
        ok, token = contains_all(g2.output, required_g2)
        if not ok:
            checks.append(f"glsl2llvm output missing substring: {token!r}")

        required_gs = case.get("expect_glslang_error_substr", [])
        ok, token = contains_all(gs.output, required_gs)
        if not ok:
            checks.append(f"glslang output missing substring: {token!r}")

        passed = len(checks) == 0
        has_failures = has_failures or (not passed)

        results.append(
            CaseResult(
                name=case_name,
                shader=str(shader_path),
                glsl2llvm=g2,
                glslang=gs,
                passed=passed,
                checks=checks,
            )
        )

    report_path = Path(args.report).resolve()
    render_report(results, report_path, glsl2llvm_exe, glslang_exe)

    passed_count = sum(1 for item in results if item.passed)
    print(
        f"differential tests: {passed_count}/{len(results)} passed; report: {report_path}",
        file=sys.stderr,
    )

    return 1 if has_failures else 0


if __name__ == "__main__":
    sys.exit(main())
