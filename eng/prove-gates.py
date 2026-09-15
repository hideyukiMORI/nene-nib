"""Exercise the real compiler/lint/format/CMake/linker paths, including restoration (QLT-007).

Every probe must fail with the diagnostic that belongs to the rule it claims to prove, and the
restored tree must build again. A setting that exists is not proof that a check works (P-09).
The evidence written to out/proofs/results.json is what docs/quality/gate-proofs.md quotes.
"""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str], cwd: Path, succeeds: bool, diagnostic: str = "") -> dict:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    output = result.stdout + result.stderr
    if (result.returncode == 0) != succeeds or (diagnostic and diagnostic not in output):
        raise RuntimeError(f"Unexpected gate proof: {command}\n{output}")
    return {"command": command, "exitCode": result.returncode, "output": output}


def copy_repository_files(root: Path) -> None:
    files = ["CMakeLists.txt", "eng/targets.cmake", "eng/architecture.json", "eng/symbol-allowlist.json",
             "eng/symbols.py", ".clang-tidy", ".clang-format", "tests/build/ToolchainSmoke.cpp"]
    for folder in ["src", "tests/unit", "tests/adapters"]:
        if (ROOT / folder).is_dir():
            files += [p.relative_to(ROOT).as_posix() for p in (ROOT / folder).rglob("*") if p.is_file()]
    for path in files:
        destination = root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / path, destination)


# Phase 0 で未実測だった clang-tidy の 2 検査。発火することはここで確かめるが、どの規則 ID に紐づくかは
# 設計リナが CODING_RULES.md で決める（NewDeleteLeaks / use-after-move は Loupe の規則表に対応が無い）。

# (規則 ID, 反例, 落とす道具が出す診断名[, 注記]). ビルド 1 回で compile と clang-tidy の両方が走る。
BUILD_PROBES = [
    ("QLT-002",
     "int main()\n{\n    int unused;\n    return 0;\n}\n",
     "-Wunused-variable"),
    ("QLT-002",
     "int lonely()\n{\n    return 1;\n}\n\nint main()\n{\n    return lonely() - 1;\n}\n",
     "-Wmissing-prototypes"),
    ("CPP-002",
     "enum class Mode\n{\n    rgb,\n    hex\n};\n\nstatic int pick(Mode mode)\n{\n    switch (mode)\n    {\n"
     "    case Mode::rgb:\n        return 0;\n    default:\n        return 1;\n    }\n}\n\n"
     "int main()\n{\n    return pick(Mode::rgb);\n}\n",
     "-Wswitch-enum"),
    ("CPP-002",
     "enum class Mode\n{\n    rgb,\n    hex\n};\n\nstatic int pick(Mode mode)\n{\n    switch (mode)\n    {\n"
     "    case Mode::rgb:\n        return 0;\n    case Mode::hex:\n        return 1;\n    default:\n"
     "        return 2;\n    }\n}\n\nint main()\n{\n    return pick(Mode::rgb);\n}\n",
     "-Wcovered-switch-default"),
    ("CPP-003",
     "struct Sample\n{\n    int value;\n};\n\nstatic int mutate(const Sample& sample)\n{\n"
     "    ((Sample&)sample).value = 2;\n    return sample.value;\n}\n\n"
     "int main()\n{\n    Sample sample{1};\n    return mutate(sample) - 2;\n}\n",
     "-Wold-style-cast"),
    ("CPP-003",
     "static int mutate(const int& value)\n{\n    const_cast<int&>(value) = 2;\n    return value;\n}\n\n"
     "int main()\n{\n    int value = 1;\n    return mutate(value) - 2;\n}\n",
     "cppcoreguidelines-pro-type-const-cast"),
    ("CPP-004",
     "int main()\n{\n    int* pointer = nullptr;\n    return *pointer;\n}\n",
     "clang-analyzer-core.NullDereference"),
    ("CPP-004",
     "#include <optional>\n\nstatic int read(std::optional<int> value)\n{\n    return value.value();\n}\n\n"
     "int main()\n{\n    return read(std::optional<int>(0));\n}\n",
     "bugprone-unchecked-optional-access"),
    ("CPP-016",
     "#include <string>\n#include <utility>\n\nint main()\n{\n    std::string first = \"x\";\n"
     "    std::string second = std::move(first);\n"
     "    return static_cast<int>(first.size() + second.size()) - 1;\n}\n",
     "bugprone-use-after-move"),  # CPP-016: 所有権を失った値の再利用
    ("CPP-012",
     "static int pick(int a, int b, int c, int d, int e)\n{\n    return a + b + c + d + e;\n}\n\n"
     "int main()\n{\n    return pick(0, 0, 0, 0, 0);\n}\n",
     "readability-function-size"),
    ("CPP-016",
     "int main(int argc, char** argv)\n{\n    static_cast<void>(argv);\n    int values[argc];\n"
     "    values[0] = 0;\n    return values[0];\n}\n",
     # C++ の VLA を拒否するのは -Werror=vla ではなく、既定で有効な vla-cxx-extension を /WX が
     # エラーへ上げる経路（C1-vla-clang）。clang-tidy 側は clang-diagnostic- 接頭辞で同じものを出す。
     "vla-cxx-extension"),
    ("CPP-018",
     "#include <immintrin.h>\n\nint main()\n{\n    __m256i first = _mm256_set1_epi32(1);\n"
     "    __m256i sum = _mm256_add_epi32(first, first);\n    return _mm256_extract_epi32(sum, 0) - 2;\n}\n",
     "requires target feature"),  # S1: /arch 無しの AVX2 は clang-cl が always_inline で拒否する
    ("ARC-005",
     "int counter = 0;\n\nint main()\n{\n    counter += 1;\n    return counter - 1;\n}\n",
     "cppcoreguidelines-avoid-non-const-global-variables"),
    ("CPP-016",
     "static int leaked()\n{\n    int* value = new int(1);\n    return *value;\n}\n\n"
     "int main()\n{\n    return leaked() - 1;\n}\n",
     "clang-analyzer-cplusplus.NewDeleteLeaks"),  # CPP-016: 所有者の無い new
]

BASELINE_PROBE = ("#include <cstring>\n"
                  "int nenenib_same(const char* a, const char* b);\n"
                  "int nenenib_same(const char* a, const char* b)\n"
                  "{\n    return std::memcmp(a, b, std::strlen(a)) == 0 ? 1 : 0;\n}\n")

# (規則 ID, 反例, symbols.py が出す診断の一部). 中核 (core) の翻訳単位が外へ要求するシンボルで判定する。
SYMBOL_PROBES = [
    ("ARC-007",
     "#include <chrono>\n"
     "long long nenenib_now();\n"
     "long long nenenib_now()\n{\n    return std::chrono::system_clock::now().time_since_epoch().count();\n}\n",
     "ARC-007: core: non-deterministic input symbol _Xtime_get_ticks"),
    ("ARC-007",
     "#include <windows.h>\n"
     "unsigned long nenenib_tick();\n"
     "unsigned long nenenib_tick()\n{\n    return GetTickCount();\n}\n",
     "ARC-007: core: non-deterministic input symbol __imp_GetTickCount"),
    ("ARC-003",
     "#include <windows.h>\n"
     "void* nenenib_open();\n"
     "void* nenenib_open()\n{\n    return CreateFileW(L\"x\", 0, 0, nullptr, 3, 0, nullptr);\n}\n",
     "ARC-003: core: undeclared external symbol __imp_CreateFileW"),
    ("ARC-003",
     "#include <filesystem>\n"
     "bool nenenib_present(const wchar_t* path);\n"
     "bool nenenib_present(const wchar_t* path)\n{\n    return std::filesystem::exists(path);\n}\n",
     "ARC-003: core: undeclared external symbol __std_fs_get_stats"),
    ("CPP-013",
     "#include <thread>\n"
     "void nenenib_spawn();\n"
     "void nenenib_spawn()\n{\n    std::thread worker([] {});\n    worker.join();\n}\n",
     "CPP-013: core: concurrency symbol outside the worker adapter _beginthreadex"),
    ("CPP-013",
     "#include <mutex>\n"
     "int nenenib_guard();\n"
     "int nenenib_guard()\n{\n    static std::mutex lock;\n    const std::lock_guard<std::mutex> guard(lock);\n"
     "    return 1;\n}\n",
     "CPP-013: core: concurrency symbol outside the worker adapter _Mtx_lock"),
    ("CPP-013",
     "#include <windows.h>\n"
     "void* nenenib_thread();\n"
     "void* nenenib_thread()\n{\n    return CreateThread(nullptr, 0, nullptr, nullptr, 0, nullptr);\n}\n",
     "CPP-013: core: concurrency symbol outside the worker adapter __imp_CreateThread"),
]


def prove_build_probes(root: Path, source: Path, original: str, build: list[str], evidence: list) -> None:
    for probe in BUILD_PROBES:
        rule, text, diagnostic = probe[0], probe[1], probe[2]
        note = probe[3] if len(probe) > 3 else ""
        source.write_text(text, encoding="utf-8")
        result = run(build, root, False, diagnostic)
        source.write_text(original, encoding="utf-8")
        restoration = run(build, root, True)
        evidence.append({"rule": rule, "diagnostic": diagnostic, "note": note, "negative": result,
                         "restorationExit": restoration["exitCode"]})
        print(f"{rule}: rejected {diagnostic}; restored build passed" + (f" [{note}]" if note else ""))


def prove_formatting(root: Path, source: Path, original: str, evidence: list) -> None:
    style = f"--style=file:{root / '.clang-format'}"
    source.write_text("int main(){return 0;}\n", encoding="utf-8")
    result = run(["clang-format", "--dry-run", "--Werror", style, str(source)], root, False,
                 "clang-format-violations")
    source.write_text(original, encoding="utf-8")
    run(["clang-format", "--dry-run", "--Werror", style, str(source)], root, True)
    evidence.append({"rule": "QLT-004", "diagnostic": "clang-format-violations",
                     "negative": result, "restorationExit": 0})
    print("QLT-004: clang-format rejected the reformatted file; restoration passed")


def prove_architecture(root: Path, configure: list[str], build: list[str], evidence: list) -> None:
    cmake_file = root / "CMakeLists.txt"
    original = cmake_file.read_text(encoding="utf-8")
    additions = [
        ("undeclared module dependency",
         "\nnenenib_target(other verification STATIC tests/build/ToolchainSmoke.cpp)\n"
         "nenenib_link(toolchain_smoke other)\n"),
        ("undeclared platform library", "\nnenenib_system_link(toolchain_smoke user32)\n"),
    ]
    for label, addition in additions:
        cmake_file.write_text(original + addition, encoding="utf-8")
        result = run(configure, root, False, "ARC-002")
        cmake_file.write_text(original, encoding="utf-8")
        run(configure, root, True)
        run(build, root, True)
        evidence.append({"rule": "ARC-002", "diagnostic": f"ARC-002 ({label})",
                         "negative": result, "restorationExit": 0})
        print(f"ARC-002: {label} rejected at configure time; restoration passed")


def prove_symbols(root: Path, evidence: list) -> None:
    probe = root / "probe.cpp"
    compile_probe = ["clang-cl", "/nologo", "/clang:-std=c++23", "/EHsc", "/W4", "/WX", "/c", str(probe),
                     f"/Fo{root / 'probe.obj'}"]
    symbols = ["python", str(root / "eng/symbols.py"), "--root", str(root),
               "--object", str(root / "probe.obj"), "--module", "core"]
    for rule, text, diagnostic in SYMBOL_PROBES:
        probe.write_text(text, encoding="utf-8")
        run(compile_probe, root, True)
        result = run(symbols, root, False, diagnostic)
        probe.write_text(BASELINE_PROBE, encoding="utf-8")
        run(compile_probe, root, True)
        restoration = run(symbols, root, True)
        evidence.append({"rule": rule, "diagnostic": diagnostic, "negative": result,
                         "restorationExit": restoration["exitCode"]})
        print(f"{rule}: llvm-nm symbol check rejected {diagnostic.split()[-1]}; the baseline object passed")
    probe.write_text(BASELINE_PROBE, encoding="utf-8")
    run(compile_probe, root, True)
    result = run(symbols + ["--require", "application"], root, False,
                 "required module application has no static library")
    restoration = run(symbols + ["--require", "core"], root, True)
    evidence.append({"rule": "ARC-003", "diagnostic": "required module application has no static library",
                     "negative": result, "restorationExit": restoration["exitCode"]})
    print("ARC-003: a required module without a static library is rejected; the present module passes")


# ADR 0007: --require を結線したので、負の証明を probe.obj ではなく**実ライブラリ**でも取り直す。
# 中核の正典ソースに非決定入力を足して実際に nenenib_core.lib を作り、それが落ちることを見る。
REAL_LIBRARY_PROBE = (
    "\nnamespace nenenib::core\n{\nlong long nenenib_probe_ticks();\n"
    "long long nenenib_probe_ticks()\n{\n"
    "    return std::chrono::system_clock::now().time_since_epoch().count();\n}\n} // namespace nenenib::core\n"
)


def prove_real_libraries(root: Path, configure: list[str], evidence: list) -> None:
    query = root / "build/.cmake/api/v1/query"
    query.mkdir(parents=True, exist_ok=True)
    (query / "codemodel-v2").write_text("", encoding="utf-8")
    run(configure, root, True)
    build = ["cmake", "--build", "build", "--target", "nenenib_core", "nenenib_application"]
    run(build, root, True)
    symbols = ["python", str(root / "eng/symbols.py"), "--root", str(root),
               "--build-dir", str(root / "build"), "--require", "core", "application"]
    run(symbols, root, True, "Symbols: 2 libraries checked, 0 violation(s)")
    source = root / "src/core/Palette.cpp"
    original = source.read_text(encoding="utf-8")
    source.write_text("#include <chrono>\n" + original + REAL_LIBRARY_PROBE, encoding="utf-8")
    run(build, root, True)
    diagnostic = "ARC-007: core: non-deterministic input symbol _Xtime_get_ticks"
    result = run(symbols, root, False, diagnostic)
    source.write_text(original, encoding="utf-8")
    run(build, root, True)
    restoration = run(symbols, root, True)
    evidence.append({"rule": "ARC-007", "diagnostic": diagnostic + " (real static library)",
                     "negative": result, "restorationExit": restoration["exitCode"]})
    print("ARC-007: the real nenenib_core.lib is checked and rejected; the restored library passed")


# QLT-014 (ADR 0011): 基準値の複製を 1 本だけ厳しくすると eng/measure-speed.py --check が落ちる。
# 証明用ツリーには exe が無いので、既に測った値と基準値だけで判定する経路で見る（窓は開かない）。
def prove_speed_reference(root: Path, evidence: list) -> None:
    reference = json.loads((ROOT / "eng/perf-reference.json").read_text(encoding="utf-8"))
    names = list(reference["benches"])
    identity = {"fingerprint": "proof-machine", "cpu": "proof", "gpu": "proof", "dpi": 96}
    record = {"recordedAt": "proof", "repetitions": 5, "machine": identity,
              "values": {name: {"medianMs": 100.0, "minimumMs": 100.0, "maximumMs": 100.0}
                         for name in names}}
    values_file = root / "speed-values.json"
    values_file.write_text(json.dumps(record), encoding="utf-8")
    reference["machines"] = {"proof-machine": dict(identity, recordedAt="proof",
                                                   values={name: {"medianMs": 100.0}
                                                           for name in names})}
    reference_file = root / "speed-reference.json"
    command = ["python", str(ROOT / "eng/measure-speed.py"), "--check",
               "--reference", str(reference_file), "--values", str(values_file)]
    reference["machines"]["proof-machine"]["values"][names[0]] = {"medianMs": 10.0}
    reference_file.write_text(json.dumps(reference), encoding="utf-8")
    diagnostic = f"QLT-014: {names[0]}"
    result = run(command, root, False, diagnostic)
    reference["machines"]["proof-machine"]["values"][names[0]] = {"medianMs": 100.0}
    reference_file.write_text(json.dumps(reference), encoding="utf-8")
    restoration = run(command, root, True, "0 regression(s)")
    evidence.append({"rule": "QLT-014", "diagnostic": diagnostic + " (reference tightened by one bench)",
                     "negative": result, "restorationExit": restoration["exitCode"]})
    print("QLT-014: a reference tightened on one bench is rejected; the untouched copy passed")


def main() -> None:
    output_root = (ROOT / "out/proofs").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    evidence: list = []
    with tempfile.TemporaryDirectory(prefix="build-", dir=output_root) as temporary:
        root = Path(temporary).resolve()
        if not root.is_relative_to(output_root):
            raise RuntimeError("Proof workspace escaped the intended output directory")
        copy_repository_files(root)
        source = root / "tests/build/ToolchainSmoke.cpp"
        original = source.read_text(encoding="utf-8")
        configure = ["cmake", "-S", ".", "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"]
        build = ["cmake", "--build", "build", "--target", "toolchain_smoke", "--clean-first"]
        run(configure, root, True)
        run(build, root, True)
        prove_build_probes(root, source, original, build, evidence)
        prove_formatting(root, source, original, evidence)
        prove_architecture(root, configure, build, evidence)
        prove_symbols(root, evidence)
        prove_real_libraries(root, configure, evidence)
        prove_speed_reference(root, evidence)
    (output_root / "results.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n",
                                              encoding="utf-8")
    print(f"Gate proofs passed: {len(evidence)} real-tool proofs")


if __name__ == "__main__":
    main()
